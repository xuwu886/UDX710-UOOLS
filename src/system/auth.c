/**
 * @file auth.c
 * @brief 后台认证模块实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "auth.h"
#include "sha256.h"

/* 外部函数声明 - 来自sms.c */
extern int config_get(const char *key, char *value, size_t value_size);
extern int config_set(const char *key, const char *value);
extern int config_get_int(const char *key, int default_val);
extern int config_set_int(const char *key, int value);
extern long long config_get_ll(const char *key, long long default_val);
extern int config_set_ll(const char *key, long long value);

/* 配置键名 */
#define KEY_PASSWORD_HASH   "auth_password_hash"
#define KEY_TOKEN           "auth_token"
#define KEY_TOKEN_EXPIRE    "auth_token_expire"

/**
 * 生成随机Token
 */
static int generate_token(char *token, size_t size)
{
    if (size < AUTH_TOKEN_SIZE) return -1;
    
    uint8_t random_bytes[32];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* 备用方案：使用时间和进程ID */
        srand((unsigned int)(time(NULL) ^ getpid()));
        for (int i = 0; i < 32; i++) {
            random_bytes[i] = (uint8_t)(rand() & 0xFF);
        }
    } else {
        ssize_t n = read(fd, random_bytes, 32);
        close(fd);
        if (n != 32) {
            return -1;
        }
    }
    
    /* 转换为hex字符串 */
    for (int i = 0; i < 32; i++) {
        sprintf(token + (i * 2), "%02x", random_bytes[i]);
    }
    token[64] = '\0';
    
    return 0;
}

/**
 * 验证密码
 */
static int verify_password(const char *password)
{
    char stored_hash[SHA256_HEX_SIZE] = {0};
    char input_hash[SHA256_HEX_SIZE] = {0};
    
    /* 获取存储的密码哈希 */
    if (config_get(KEY_PASSWORD_HASH, stored_hash, sizeof(stored_hash)) != 0) {
        return -1;  /* 未设置密码 */
    }
    
    /* 计算输入密码的哈希 */
    sha256_hash_string(password, input_hash);
    
    /* 比较哈希值 */
    return (strcmp(stored_hash, input_hash) == 0) ? 0 : -1;
}

int auth_init(void)
{
    char hash[SHA256_HEX_SIZE] = {0};
    char token[AUTH_TOKEN_SIZE] = {0};
    long long expire_time;
    
    printf("[AUTH] 初始化认证模块\n");
    
    /* 检查是否已设置密码 */
    if (config_get(KEY_PASSWORD_HASH, hash, sizeof(hash)) != 0 || strlen(hash) == 0) {
        /* 设置默认密码 */
        printf("[AUTH] 设置默认密码: %s\n", AUTH_DEFAULT_PASSWORD);
        sha256_hash_string(AUTH_DEFAULT_PASSWORD, hash);
        if (config_set(KEY_PASSWORD_HASH, hash) != 0) {
            printf("[AUTH] 设置默认密码失败\n");
            return -1;
        }
    }
    
    /* 启动时清理过期Token */
    if (config_get(KEY_TOKEN, token, sizeof(token)) == 0 && strlen(token) > 0) {
        expire_time = config_get_ll(KEY_TOKEN_EXPIRE, 0);
        if (expire_time == 0 || (long long)time(NULL) > expire_time) {
            printf("[AUTH] 清理过期Token\n");
            config_set(KEY_TOKEN, "");
            config_set_ll(KEY_TOKEN_EXPIRE, 0);
        }
    }
    
    printf("[AUTH] 认证模块初始化完成\n");
    return 0;
}

int auth_login(const char *password, char *token, size_t token_size)
{
    if (!password || !token || token_size < AUTH_TOKEN_SIZE) {
        return -2;
    }
    
    printf("[AUTH] 尝试登录\n");
    
    /* 验证密码 */
    if (verify_password(password) != 0) {
        printf("[AUTH] 密码错误\n");
        return -1;
    }
    
    /* 生成新Token */
    if (generate_token(token, token_size) != 0) {
        printf("[AUTH] 生成Token失败\n");
        return -2;
    }
    
    /* 保存Token */
    if (config_set(KEY_TOKEN, token) != 0) {
        printf("[AUTH] 保存Token失败\n");
        return -2;
    }
    
    /* 设置过期时间 */
    long long expire_time = (long long)time(NULL) + AUTH_TOKEN_EXPIRE_SECONDS;
    if (config_set_ll(KEY_TOKEN_EXPIRE, expire_time) != 0) {
        printf("[AUTH] 设置过期时间失败\n");
        return -2;
    }
    
    printf("[AUTH] 登录成功，Token有效期: %d秒\n", AUTH_TOKEN_EXPIRE_SECONDS);
    return 0;
}

int auth_verify_token(const char *token)
{
    char stored_token[AUTH_TOKEN_SIZE] = {0};
    long long expire_time;
    
    if (!token || strlen(token) == 0) {
        return -1;
    }
    
    /* 获取存储的Token */
    if (config_get(KEY_TOKEN, stored_token, sizeof(stored_token)) != 0) {
        return -1;  /* 无Token */
    }
    
    /* 比较Token */
    if (strcmp(token, stored_token) != 0) {
        return -1;  /* Token不匹配 */
    }
    
    /* 检查过期时间 */
    expire_time = config_get_ll(KEY_TOKEN_EXPIRE, 0);
    if (expire_time == 0 || (long long)time(NULL) > expire_time) {
        printf("[AUTH] Token已过期，自动清除\n");
        /* 清除过期Token */
        config_set(KEY_TOKEN, "");
        config_set_ll(KEY_TOKEN_EXPIRE, 0);
        return -1;  /* Token已过期 */
    }
    
    return 0;
}

int auth_change_password(const char *old_password, const char *new_password)
{
    char new_hash[SHA256_HEX_SIZE] = {0};
    
    if (!old_password || !new_password) {
        return -2;
    }
    
    if (strlen(new_password) < 1) {
        printf("[AUTH] 新密码不能为空\n");
        return -2;
    }
    
    printf("[AUTH] 尝试修改密码\n");
    
    /* 验证旧密码 */
    if (verify_password(old_password) != 0) {
        printf("[AUTH] 旧密码错误\n");
        return -1;
    }
    
    /* 计算新密码哈希 */
    sha256_hash_string(new_password, new_hash);
    
    /* 保存新密码 */
    if (config_set(KEY_PASSWORD_HASH, new_hash) != 0) {
        printf("[AUTH] 保存新密码失败\n");
        return -2;
    }
    
    /* 清除当前Token，强制重新登录 */
    config_set(KEY_TOKEN, "");
    config_set_ll(KEY_TOKEN_EXPIRE, 0);
    
    printf("[AUTH] 密码修改成功\n");
    return 0;
}

int auth_logout(const char *token)
{
    char stored_token[AUTH_TOKEN_SIZE] = {0};
    
    if (!token) {
        return -1;
    }
    
    /* 获取存储的Token */
    if (config_get(KEY_TOKEN, stored_token, sizeof(stored_token)) != 0) {
        return -1;
    }
    
    /* 验证Token */
    if (strcmp(token, stored_token) != 0) {
        return -1;
    }
    
    /* 清除Token */
    config_set(KEY_TOKEN, "");
    config_set_ll(KEY_TOKEN_EXPIRE, 0);
    
    printf("[AUTH] 登出成功\n");
    return 0;
}

int auth_get_status(int *logged_in)
{
    char token[AUTH_TOKEN_SIZE] = {0};
    long long expire_time;
    
    if (!logged_in) {
        return -1;
    }
    
    *logged_in = 0;
    
    /* 检查是否有有效Token */
    if (config_get(KEY_TOKEN, token, sizeof(token)) != 0 || strlen(token) == 0) {
        return 0;
    }
    
    /* 检查过期时间 */
    expire_time = config_get_ll(KEY_TOKEN_EXPIRE, 0);
    if (expire_time > 0 && (long long)time(NULL) <= expire_time) {
        *logged_in = 1;
    } else if (strlen(token) > 0) {
        /* Token已过期，自动清除 */
        printf("[AUTH] 检测到过期Token，自动清除\n");
        config_set(KEY_TOKEN, "");
        config_set_ll(KEY_TOKEN_EXPIRE, 0);
    }
    
    return 0;
}

int auth_is_required(void)
{
    char hash[SHA256_HEX_SIZE] = {0};
    
    /* 如果设置了密码哈希，则需要认证 */
    if (config_get(KEY_PASSWORD_HASH, hash, sizeof(hash)) == 0 && strlen(hash) > 0) {
        return 1;
    }
    
    return 0;
}
