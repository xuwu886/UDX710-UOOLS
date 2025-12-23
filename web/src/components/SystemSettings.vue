<script setup>
import { ref, onMounted, onUnmounted, inject } from 'vue'
import { useI18n } from 'vue-i18n'
import { deviceControl, getRebootConfig, setReboot, clearReboot, getSystemTime, syncSystemTime, useApi, authChangePassword } from '../composables/useApi'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { t } = useI18n()
const { success, error } = useToast()
const { confirm } = useConfirm()
const api = useApi()

// 获取登出函数
const handleLogout = inject('handleLogout', () => {})

const rebooting = ref(false)
const shuttingDown = ref(false)
const rebootEnabled = ref(false)
const rebootTime = ref('')
const selectedDays = ref([])
const savingReboot = ref(false)
const currentTime = ref('')
const syncingTime = ref(false)

// 密码修改相关
const oldPassword = ref('')
const newPassword = ref('')
const confirmPassword = ref('')
const changingPassword = ref(false)
const showPasswordForm = ref(false)

const weekDays = [
  { value: '1', labelKey: 'settings.mon', short: '一' },
  { value: '2', labelKey: 'settings.tue', short: '二' },
  { value: '3', labelKey: 'settings.wed', short: '三' },
  { value: '4', labelKey: 'settings.thu', short: '四' },
  { value: '5', labelKey: 'settings.fri', short: '五' },
  { value: '6', labelKey: 'settings.sat', short: '六' },
  { value: '0', labelKey: 'settings.sun', short: '日' }
]

async function fetchRebootConfig() {
  try {
    const data = await getRebootConfig()
    if (data.success && data.job) {
      rebootEnabled.value = true
      const [minute, hour, , , days] = data.job.split(' ')
      rebootTime.value = `${hour.padStart(2, '0')}:${minute.padStart(2, '0')}`
      selectedDays.value = days.split(',')
    }
  } catch (err) {
    console.error('获取定时重启配置失败:', err)
  }
}

async function fetchSystemTime() {
  try {
    const data = await getSystemTime()
    if (data.Code === 0 && data.Data) {
      currentTime.value = data.Data.datetime
    }
  } catch (err) {
    console.error('获取系统时间失败:', err)
  }
}

async function handleSyncTime() {
  syncingTime.value = true
  try {
    const data = await syncSystemTime()
    if (data.Code === 0) {
      success(t('settings.timeSynced') + ': ' + (data.server || 'NTP'))
      await fetchSystemTime()
    } else {
      error(data.Error || t('settings.syncFailed'))
    }
  } catch (err) {
    error(t('settings.syncFailed') + ': ' + err.message)
  } finally {
    syncingTime.value = false
  }
}

// 修改密码
async function handleChangePassword() {
  if (!oldPassword.value || !newPassword.value || !confirmPassword.value) {
    error(t('auth.fillAllFields'))
    return
  }
  if (newPassword.value !== confirmPassword.value) {
    error(t('auth.passwordMismatch'))
    return
  }
  if (newPassword.value.length < 4) {
    error(t('auth.passwordTooShort'))
    return
  }
  
  changingPassword.value = true
  try {
    const result = await authChangePassword(oldPassword.value, newPassword.value)
    if (result.success) {
      success(t('auth.passwordChanged'))
      oldPassword.value = ''
      newPassword.value = ''
      confirmPassword.value = ''
      showPasswordForm.value = false
    } else {
      error(result.error || t('auth.changeFailed'))
    }
  } catch (err) {
    error(t('auth.changeFailed') + ': ' + err.message)
  } finally {
    changingPassword.value = false
  }
}

// 切换密码表单显示
function togglePasswordForm() {
  showPasswordForm.value = !showPasswordForm.value
  if (!showPasswordForm.value) {
    oldPassword.value = ''
    newPassword.value = ''
    confirmPassword.value = ''
  }
}

// 登出
async function doLogout() {
  if (!await confirm({ title: t('auth.logout'), message: t('auth.confirmLogout') })) return
  handleLogout()
}

async function handleReboot() {
  if (!await confirm({ title: t('settings.reboot'), message: t('settings.confirmReboot'), danger: true })) return
  rebooting.value = true
  try {
    await deviceControl('reboot')
    success(t('settings.rebootSent'))
  } catch (err) {
    error(t('settings.rebootFailed') + ': ' + err.message)
  } finally {
    rebooting.value = false
  }
}

async function handleShutdown() {
  if (!await confirm({ title: t('settings.shutdown'), message: t('settings.confirmShutdown'), danger: true })) return
  shuttingDown.value = true
  try {
    await deviceControl('poweroff')
    success(t('settings.shutdownSent'))
  } catch (err) {
    error(t('settings.shutdownFailed') + ': ' + err.message)
  } finally {
    shuttingDown.value = false
  }
}

function toggleAllDays() {
  if (selectedDays.value.length === 7) {
    selectedDays.value = []
  } else {
    selectedDays.value = ['1', '2', '3', '4', '5', '6', '0']
  }
}

async function saveRebootConfig() {
  if (rebootEnabled.value) {
    if (!rebootTime.value) {
      error(t('settings.selectTime'))
      return
    }
    if (selectedDays.value.length === 0) {
      error(t('settings.selectDays'))
      return
    }
    savingReboot.value = true
    try {
      const [hour, minute] = rebootTime.value.split(':')
      await setReboot(selectedDays.value, hour, minute)
      success(t('settings.rebootConfigSaved'))
    } catch (err) {
      error(t('settings.saveFailed') + ': ' + err.message)
    } finally {
      savingReboot.value = false
    }
  } else {
    savingReboot.value = true
    try {
      await clearReboot()
      success(t('settings.rebootCleared'))
    } catch (err) {
      error(t('settings.clearFailed') + ': ' + err.message)
    } finally {
      savingReboot.value = false
    }
  }
}

// 每秒更新系统时间（从API获取）
let timeInterval = null

onMounted(() => {
  fetchRebootConfig()
  fetchSystemTime()
  timeInterval = setInterval(fetchSystemTime, 1000)
})

onUnmounted(() => {
  if (timeInterval) {
    clearInterval(timeInterval)
  }
})
</script>

<template>
  <div class="space-y-4 sm:space-y-6">
    <!-- 设备控制 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-6 flex items-center">
        <i class="fas fa-power-off text-red-500 dark:text-red-400 mr-2"></i>
        {{ t('settings.deviceControl') }}
      </h3>
      
      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 重启按钮 -->
        <button 
          @click="handleReboot" 
          :disabled="rebooting || shuttingDown"
          class="group relative overflow-hidden p-6 rounded-2xl bg-gradient-to-br from-blue-500/20 to-cyan-500/20 border border-blue-500/30 hover:border-blue-500/50 transition-all disabled:opacity-50"
        >
          <div class="absolute inset-0 bg-gradient-to-r from-blue-500/0 via-blue-500/10 to-blue-500/0 translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-1000"></div>
          <div class="relative flex items-center space-x-4">
            <div class="w-16 h-16 rounded-2xl bg-gradient-to-br from-blue-500 to-cyan-400 flex items-center justify-center shadow-lg shadow-blue-500/30">
              <i :class="rebooting ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'" class="text-white text-2xl"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-bold text-xl">{{ rebooting ? t('settings.rebooting') : t('settings.reboot') }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm mt-1">{{ t('settings.rebootDesc') }}</p>
            </div>
          </div>
        </button>

        <!-- 关机按钮 -->
        <button 
          @click="handleShutdown" 
          :disabled="rebooting || shuttingDown"
          class="group relative overflow-hidden p-6 rounded-2xl bg-gradient-to-br from-red-500/20 to-orange-500/20 border border-red-500/30 hover:border-red-500/50 transition-all disabled:opacity-50"
        >
          <div class="absolute inset-0 bg-gradient-to-r from-red-500/0 via-red-500/10 to-red-500/0 translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-1000"></div>
          <div class="relative flex items-center space-x-4">
            <div class="w-16 h-16 rounded-2xl bg-gradient-to-br from-red-500 to-orange-400 flex items-center justify-center shadow-lg shadow-red-500/30">
              <i :class="shuttingDown ? 'fas fa-spinner animate-spin' : 'fas fa-power-off'" class="text-white text-2xl"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-bold text-xl">{{ shuttingDown ? t('settings.shuttingDown') : t('settings.shutdown') }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm mt-1">{{ t('settings.shutdownDesc') }}</p>
            </div>
          </div>
        </button>
      </div>

      <div class="mt-6 p-3 bg-yellow-500/10 border border-yellow-500/20 rounded-xl">
        <p class="text-yellow-600 dark:text-yellow-400 text-sm flex items-center">
          <i class="fas fa-exclamation-triangle mr-2"></i>
          {{ t('settings.operationWarning') }}
        </p>
      </div>
    </div>

    <!-- 定时重启 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-6 flex items-center">
        <i class="fas fa-clock text-amber-500 dark:text-amber-400 mr-2"></i>
        {{ t('settings.scheduledReboot') }}
      </h3>

      <!-- 系统时间 -->
      <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-lg bg-amber-500/20 flex items-center justify-center">
            <i class="fas fa-calendar-alt text-amber-500 dark:text-amber-400"></i>
          </div>
          <div>
            <p class="text-slate-500 dark:text-white/50 text-xs">{{ t('settings.systemTime') }}</p>
            <p class="text-slate-900 dark:text-white font-medium">{{ currentTime || t('common.loading') }}</p>
          </div>
        </div>
        <button 
          @click="handleSyncTime" 
          :disabled="syncingTime"
          class="px-4 py-2 bg-gradient-to-r from-blue-500 to-cyan-400 text-white text-sm font-medium rounded-lg hover:shadow-lg hover:shadow-blue-500/30 transition-all disabled:opacity-50 flex items-center"
        >
          <i :class="syncingTime ? 'fas fa-spinner fa-spin' : 'fas fa-sync-alt'" class="mr-2"></i>
          {{ syncingTime ? t('settings.syncing') : t('settings.ntpSync') }}
        </button>
      </div>

      <!-- 启用开关 -->
      <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-lg bg-green-500/20 flex items-center justify-center">
            <i class="fas fa-toggle-on text-green-600 dark:text-green-400"></i>
          </div>
          <div>
            <p class="text-slate-900 dark:text-white font-medium">{{ t('settings.enableScheduledReboot') }}</p>
            <p class="text-slate-500 dark:text-white/50 text-sm">{{ t('settings.enableScheduledRebootDesc') }}</p>
          </div>
        </div>
        <label class="relative cursor-pointer">
          <input type="checkbox" v-model="rebootEnabled" class="sr-only peer">
          <div class="w-14 h-7 bg-slate-200 dark:bg-white/20 rounded-full peer peer-checked:bg-green-500 transition-colors"></div>
          <div class="absolute top-0.5 left-0.5 w-6 h-6 bg-white rounded-full shadow transition-transform peer-checked:translate-x-7"></div>
        </label>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 重启时间 -->
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">{{ t('settings.rebootTime') }}</label>
          <input 
            type="time" 
            v-model="rebootTime" 
            :disabled="!rebootEnabled"
            class="w-full px-4 py-3 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white focus:border-amber-400 focus:ring-2 focus:ring-amber-400/20 transition-all disabled:opacity-50"
          >
        </div>

        <!-- 重启日期 -->
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">{{ t('settings.rebootDays') }}</label>
          <div class="flex flex-wrap gap-2">
            <button 
              @click="toggleAllDays"
              :disabled="!rebootEnabled"
              class="px-3 py-2 rounded-lg text-sm transition-all disabled:opacity-50"
              :class="selectedDays.length === 7 ? 'bg-amber-500 text-white' : 'bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-200 dark:hover:bg-white/20'"
            >
              {{ t('settings.selectAll') }}
            </button>
            <button
              v-for="day in weekDays"
              :key="day.value"
              @click="selectedDays.includes(day.value) ? selectedDays = selectedDays.filter(d => d !== day.value) : selectedDays.push(day.value)"
              :disabled="!rebootEnabled"
              class="w-10 h-10 rounded-lg text-sm transition-all disabled:opacity-50"
              :class="selectedDays.includes(day.value) ? 'bg-amber-500 text-white' : 'bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-200 dark:hover:bg-white/20'"
            >
              {{ day.short }}
            </button>
          </div>
        </div>
      </div>

      <!-- 保存按钮 -->
      <button 
        @click="saveRebootConfig" 
        :disabled="savingReboot"
        class="w-full mt-6 py-3 bg-gradient-to-r from-amber-500 to-orange-400 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-amber-500/30 transition-all disabled:opacity-50"
      >
        <i :class="savingReboot ? 'fas fa-spinner animate-spin' : 'fas fa-save'" class="mr-2"></i>
        {{ savingReboot ? t('settings.saving') : t('settings.saveSettings') }}
      </button>
    </div>

    <!-- 账户安全 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-6 flex items-center">
        <i class="fas fa-shield-alt text-green-500 dark:text-green-400 mr-2"></i>
        {{ t('auth.accountSecurity') }}
      </h3>

      <div class="space-y-4">
        <!-- 修改密码按钮 -->
        <button 
          @click="togglePasswordForm"
          class="w-full flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl hover:bg-slate-100 dark:hover:bg-white/10 transition-all"
        >
          <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-lg bg-amber-500/20 flex items-center justify-center">
              <i class="fas fa-key text-amber-500 dark:text-amber-400"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-medium">{{ t('auth.changePassword') }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm">{{ t('auth.changePasswordDesc') || '修改登录密码' }}</p>
            </div>
          </div>
          <i :class="showPasswordForm ? 'fas fa-chevron-up' : 'fas fa-chevron-down'" class="text-slate-400 dark:text-white/40 transition-transform duration-300"></i>
        </button>

        <!-- 密码修改表单 - 带动画 -->
        <Transition
          enter-active-class="transition-all duration-300 ease-out"
          enter-from-class="opacity-0 -translate-y-2"
          enter-to-class="opacity-100 translate-y-0"
          leave-active-class="transition-all duration-200 ease-in"
          leave-from-class="opacity-100 translate-y-0"
          leave-to-class="opacity-0 -translate-y-2"
        >
          <div v-if="showPasswordForm" class="space-y-4 pl-4 border-l-2 border-amber-500/30 ml-5 overflow-hidden">
            <div>
              <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">{{ t('auth.oldPassword') }}</label>
              <input 
                type="password" 
                v-model="oldPassword"
                :placeholder="t('auth.enterOldPassword')"
                class="w-full px-4 py-3 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white focus:border-green-400 focus:ring-2 focus:ring-green-400/20 transition-all"
              >
            </div>
            
            <div>
              <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">{{ t('auth.newPassword') }}</label>
              <input 
                type="password" 
                v-model="newPassword"
                :placeholder="t('auth.enterNewPassword')"
                class="w-full px-4 py-3 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white focus:border-green-400 focus:ring-2 focus:ring-green-400/20 transition-all"
              >
            </div>
            
            <div>
              <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">{{ t('auth.confirmNewPassword') }}</label>
              <input 
                type="password" 
                v-model="confirmPassword"
                :placeholder="t('auth.enterConfirmPassword')"
                class="w-full px-4 py-3 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white focus:border-green-400 focus:ring-2 focus:ring-green-400/20 transition-all"
              >
            </div>
            
            <button 
              @click="handleChangePassword" 
              :disabled="changingPassword"
              class="w-full py-3 bg-gradient-to-r from-green-500 to-emerald-400 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-green-500/30 transition-all disabled:opacity-50"
            >
              <i :class="changingPassword ? 'fas fa-spinner animate-spin' : 'fas fa-check'" class="mr-2"></i>
              {{ changingPassword ? t('common.loading') : t('common.confirm') }}
            </button>
          </div>
        </Transition>

        <!-- 登出按钮 -->
        <button 
          @click="doLogout"
          class="w-full flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl hover:bg-red-50 dark:hover:bg-red-500/10 transition-all group"
        >
          <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-lg bg-slate-200 dark:bg-white/10 group-hover:bg-red-500/20 flex items-center justify-center transition-colors">
              <i class="fas fa-sign-out-alt text-slate-500 dark:text-white/50 group-hover:text-red-500 dark:group-hover:text-red-400 transition-colors"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-medium group-hover:text-red-600 dark:group-hover:text-red-400 transition-colors">{{ t('auth.logout') }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm">{{ t('auth.logoutDesc') }}</p>
            </div>
          </div>
          <i class="fas fa-chevron-right text-slate-400 dark:text-white/40 group-hover:text-red-500 transition-colors"></i>
        </button>
      </div>
    </div>
  </div>
</template>
