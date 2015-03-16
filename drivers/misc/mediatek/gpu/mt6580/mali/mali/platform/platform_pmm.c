/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "platform_pmm.h"
#include "mach/mt_gpufreq.h"
#include <asm/atomic.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);

/// #define __POWER_CLK_CTRL_SYNC__
/// For MFG sub-system clock control API
#include <mach/mt_clkmgr.h>
#include <linux/kernel.h>

static unsigned int current_sample_utilization = 0;

static DEFINE_SPINLOCK(mali_pwr_lock);

// Mediatek: for GPU DVFS control
atomic_t g_current_frequency_id;
atomic_t g_boost_frequency_id;
atomic_t g_perf_hal_frequency_id;
atomic_t g_ged_hal_frequency_id;
atomic_t g_current_deferred_count;
atomic_t g_input_boost_enabled;
atomic_t g_input_boost_duration;
atomic_t g_dvfs_threshold_min;
atomic_t g_dvfs_threshold_max;
atomic_t g_dvfs_deferred_count;

int mtk_get_current_frequency_id(void)
{
    return atomic_read(&g_current_frequency_id);
}

void mtk_set_current_frequency_id(int id)
{
    atomic_set(&g_current_frequency_id, id);
}

int mtk_get_boost_frequency_id(void)
{
    return atomic_read(&g_boost_frequency_id);
}

void mtk_set_boost_frequency_id(int id)
{
    atomic_set(&g_boost_frequency_id, id);
}

int mtk_get_perf_hal_frequency_id(void)
{
    return atomic_read(&g_perf_hal_frequency_id);
}

void mtk_set_perf_hal_frequency_id(int id)
{
    atomic_set(&g_perf_hal_frequency_id, id);
}

int mtk_get_ged_hal_frequency_id(void)
{
    return atomic_read(&g_ged_hal_frequency_id);
}

void mtk_set_ged_hal_frequency_id(int id)
{
    atomic_set(&g_ged_hal_frequency_id, id);
}

int mtk_get_current_deferred_count(void)
{
    return atomic_read(&g_current_deferred_count);
}

void mtk_set_current_deferred_count(int count)
{
    atomic_set(&g_current_deferred_count, count);
}

int mtk_get_input_boost_enabled(void)
{
    return atomic_read(&g_input_boost_enabled);
}

void mtk_set_input_boost_enabled(int enabled)
{
    atomic_set(&g_input_boost_enabled, enabled);
}

int mtk_get_input_boost_duration(void)
{
    return atomic_read(&g_input_boost_duration);
}

void mtk_set_input_boost_duration(int duration)
{
    atomic_set(&g_input_boost_duration, duration); 
}

int mtk_get_dvfs_threshold_min(void)
{
    return atomic_read(&g_dvfs_threshold_min);
}

void mtk_set_dvfs_threshold_min(int min)
{
    atomic_set(&g_dvfs_threshold_min, &min);
}

int mtk_get_dvfs_threshold_max(void)
{
    return atomic_read(&g_dvfs_threshold_max);
}

void mtk_set_dvfs_threshold_max(int max)
{
    atomic_set(&g_dvfs_threshold_max, max);
}

int mtk_get_dvfs_deferred_count(void)
{
    return atomic_read(&g_dvfs_deferred_count);
}

void mtk_set_dvfs_deferred_count(int count)
{
    atomic_set(&g_dvfs_deferred_count, count);
}

void mtk_gpu_input_boost_CB(unsigned int boostID)
{
    unsigned int currentID;

    if(mtk_get_input_boost_enabled() == 0)
        return;

    currentID = mt_gpufreq_get_cur_freq_index();

    MALI_DEBUG_PRINT(4, ("[MALI] current gpu freq id=%d, touch boost to index=%d\n", currentID, boostID));

    if(boostID < currentID)
    {
        if (0 == mt_gpufreq_target(boostID))
        {
            mtk_set_boost_frequency_id(boostID);
            mtk_set_current_deferred_count(0);
            mtk_set_input_boost_duration(3);
        }
    }
}


void mtk_gpu_power_limit_CB(unsigned int limitID)
{
    unsigned int currentID;

    MALI_DEBUG_PRINT(4, ("[MALI] boost CB set to freq id=%d\n", limitID));

    currentID = mt_gpufreq_get_cur_freq_index();

    if(currentID < limitID)
    {
        if (0 == mt_gpufreq_target(limitID))
        {
            mtk_set_current_frequency_id(limitID);
            mtk_set_current_deferred_count(0);
        }
    }
}

static void mtk_touch_boost_gpu_freq()
{
    mtk_gpu_input_boost_CB(0);
}


/* MTK custom boost. 0 is the lowest frequency index. The function is used for performance service currently.*/
static void mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel)
{
    unsigned int uiTableNum;
    unsigned int targetID;

    uiTableNum  = mt_gpufreq_get_dvfs_table_num();
    if (ui32FreqLevel >= uiTableNum)
    {
        ui32FreqLevel = uiTableNum - 1;
    }
    
    targetID = uiTableNum - ui32FreqLevel - 1;

    mtk_set_perf_hal_frequency_id(targetID);

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_custom_boost_gpu_freq() level=%d, boost ID=%d", ui32FreqLevel, targetID));

    if(targetID < mt_gpufreq_get_cur_freq_index())
    {
        MALI_DEBUG_PRINT(4, ("[MALI] mtk_custom_boost_gpu_freq set gpu freq to index=%d, cuurent index=%d", targetID, mt_gpufreq_get_cur_freq_index()));
        mt_gpufreq_target(targetID);
    }
}

/* MTK set boost. 0 is the lowest frequency index. The function is used for GED boost currently.*/
static void mtk_ged_bottom_gpu_freq(unsigned int ui32FreqLevel)
{
    unsigned int uiTableNum;
    unsigned int targetID;

    uiTableNum = mt_gpufreq_get_dvfs_table_num();
    if (ui32FreqLevel >= uiTableNum)
    {
        ui32FreqLevel = uiTableNum - 1;
    }

    targetID = uiTableNum - ui32FreqLevel - 1;

    mtk_set_ged_hal_frequency_id(targetID);

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_set_bottom_gpu_freq_fp() ui32FreqLevel=%d, g_custom_gpu_boost_id=%d  (GED boost)", ui32FreqLevel, targetID));

    if(targetID < mt_gpufreq_get_cur_freq_index())
    {
        MALI_DEBUG_PRINT(4, ("[MALI] mtk_set_bottom_gpu_freq_fp set gpu freq to index=%d, cuurent index=%d  (GED boost)", targetID, mt_gpufreq_get_cur_freq_index()));
        mt_gpufreq_target(targetID);
    }
}

static unsigned int mtk_custom_get_gpu_freq_level_count()
{
    return mt_gpufreq_get_dvfs_table_num();
}


void mali_pmm_init(void)
{
    MALI_DEBUG_PRINT(4, ("%s\n", __FUNCTION__));

    atomic_set(&g_current_frequency_id, 0);
    atomic_set(&g_boost_frequency_id, 0);
    atomic_set(&g_perf_hal_frequency_id, 4);
    atomic_set(&g_ged_hal_frequency_id, 4); 
    atomic_set(&g_current_deferred_count, 0);
    atomic_set(&g_input_boost_enabled, 1);
    atomic_set(&g_input_boost_duration, 0);
    atomic_set(&g_dvfs_threshold_min, 25);
    atomic_set(&g_dvfs_threshold_max, 45);
    atomic_set(&g_dvfs_deferred_count, 3);

	/* MTK Register input boost and power limit call back function */
	mt_gpufreq_input_boost_notify_registerCB(mtk_gpu_input_boost_CB);
	mt_gpufreq_power_limit_notify_registerCB(mtk_gpu_power_limit_CB);

	/* Register gpu boost function to MTK HAL */
	mtk_boost_gpu_freq_fp = mtk_touch_boost_gpu_freq;
	mtk_custom_boost_gpu_freq_fp = mtk_custom_boost_gpu_freq; /* used for for performance service boost */
    mtk_set_bottom_gpu_freq_fp = mtk_ged_bottom_gpu_freq; /* used for GED boost */
	mtk_custom_get_gpu_freq_level_count_fp = mtk_custom_get_gpu_freq_level_count;
    mtk_get_gpu_loading_fp = gpu_get_current_utilization;

    mali_platform_power_mode_change(MALI_POWER_MODE_ON);
}


void mali_pmm_deinit(void)
{
    MALI_DEBUG_PRINT(4, ("%s\n", __FUNCTION__));

    mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);
}


/* this function will be called periodically with sampling period 200ms~1000ms */
void mali_pmm_utilization_handler(struct mali_gpu_utilization_data *data)
{
    unsigned long    flags;
    mali_dvfs_action action;
    int              duration;
    int              currentID;
    int              targetID;
    int              deferred;

    current_sample_utilization = data->utilization_gpu;

    MALI_DEBUG_PRINT(4, ("%s GPU utilization=%d\n", __FUNCTION__, current_sample_utilization));

    if (!clock_is_on(MT_CG_BG3D))
    {
        MALI_DEBUG_PRINT(4, ("GPU clock is in off state\n"));
        return;
    }

    current_sample_utilization = (current_sample_utilization * 100) / 256;

    if (current_sample_utilization <= mtk_get_dvfs_threshold_min())
    {
        action = MALI_DVFS_CLOCK_DOWN;
    }
    else if (current_sample_utilization >= mtk_get_dvfs_threshold_max())
    {
        action = MALI_DVFS_CLOCK_UP;
    }
    else
    {
        MALI_DEBUG_PRINT(4, ("No need to adjust GPU frequency!\n"));
        return;
    }

    // Get current frequency id    
    currentID = mt_gpufreq_get_cur_freq_index();

    // Get current deferred count
    deferred  = mtk_get_current_deferred_count();

    switch(action)
    {
        case MALI_DVFS_CLOCK_UP:
            targetID = currentID - 1;
            deferred += 1;
            break;
        case MALI_DVFS_CLOCK_DOWN:
            targetID = currentID + 1;
            deferred += 1;
            break;
        default:
            MALI_DEBUG_PRINT(1, ("Unknown GPU DVFS operation!\n"));
            return;
    }

    // Thermal power limit
    if (targetID < mt_gpufreq_get_thermal_limit_index())
    {
        targetID = mt_gpufreq_get_thermal_limit_index();
    }

    duration = mtk_get_input_boost_duration();
    if (duration > 0)
    {
        MALI_DEBUG_PRINT(4, ("Still in the boost duration!\n"));
        
        mtk_set_input_boost_duration(duration - 1);
        
        if (targetID >= mtk_get_boost_frequency_id())
        {
            mtk_set_current_deferred_count(deferred);
            return;
        }
    }
    else if (deferred < mtk_get_dvfs_deferred_count())
    {
        MALI_DEBUG_PRINT(4, ("Defer DVFS frequency operation!\n"));
        mtk_set_current_deferred_count(deferred);
        return;
    }

    if(currentID == targetID)
    {
        MALI_DEBUG_PRINT(4, ("Target GPU frequency is the same!\n"));
        return;
    }

    if (targetID < 0)
    {
        targetID = 0;
    }
    else if (targetID >= mt_gpufreq_get_dvfs_table_num())
    {
        targetID = mt_gpufreq_get_dvfs_table_num() - 1;
    }

    mtk_set_current_frequency_id(targetID);
    
    if (targetID < mtk_get_boost_frequency_id())
    {
        mtk_set_boost_frequency_id(targetID);
    }

    mtk_set_current_deferred_count(0);
}


unsigned int gpu_get_current_utilization(void)
{
    return (current_sample_utilization * 100) / 256;
}

extern void mali_utilization_suspend(void);

void mali_SODI_begin(void)
{
    mali_utilization_suspend();
}
EXPORT_SYMBOL(mali_SODI_begin);


void mali_SODI_exit(void)
{
    atomic_set(&g_current_frequency_id, 0);
    atomic_set(&g_current_deferred_count, 0);
    atomic_set(&g_input_boost_duration, 0);
}
EXPORT_SYMBOL(mali_SODI_exit);


void mali_platform_power_mode_change(mali_power_mode power_mode)
{
    unsigned long flags;
    int           index;
    int           perfID;
    int           gedID;
    int           halID;

    switch (power_mode)
    {
        case MALI_POWER_MODE_ON:
            MALI_DEBUG_PRINT(4,("[+]MFG enable_clock \n"));

            spin_lock_irqsave(&mali_pwr_lock, flags);
            if (!clock_is_on(MT_CG_BG3D))
            {
                enable_clock(MT_CG_MFG_MM_SW_CG, "MFG");
                enable_clock(MT_CG_BG3D, "MFG");              
            }

            // Adjust GPU frequency
            if (mtk_get_input_boost_duration() > 0)
            {
                index = mtk_get_boost_frequency_id();
                mt_gpufreq_target(index);
            }
            else
            {
                // [MTK] mtk_custom_gpu_boost.
                perfID = mtk_get_perf_hal_frequency_id();
                gedID  = mtk_get_ged_hal_frequency_id();
                index  = mtk_get_current_frequency_id();

                if (index < mt_gpufreq_get_thermal_limit_index())
                {
                    index = mt_gpufreq_get_thermal_limit_index();
                }

                /* calculate higher boost frequency (e.g. lower index id) */
                if(perfID < gedID)
                {
                    halID = perfID;
                }
                else
                {
                    halID = gedID;
                }
    
                if(index > halID)
                {
                    MALI_DEBUG_PRINT(4, ("Use GPU boost frequency %d as target!\n", halID));
                    mt_gpufreq_target(halID);
                }
                else
                {
                    MALI_DEBUG_PRINT(4, ("Restore GPU frequency %d as target!\n", index));
                    mt_gpufreq_target(index);
                }
            }
            spin_unlock_irqrestore(&mali_pwr_lock, flags);
                    
            MALI_DEBUG_PRINT(4,("[-]MFG enable_clock \n"));

#if defined(CONFIG_MALI400_PROFILING)
            _mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
                                              MALI_PROFILING_EVENT_CHANNEL_GPU |
                                              MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE, 500, 1200 / 1000, 0, 0, 0);
#endif
            break;
        case MALI_POWER_MODE_LIGHT_SLEEP:
        case MALI_POWER_MODE_DEEP_SLEEP:
            MALI_DEBUG_PRINT(4,("[+]MFG disable_clock \n"));

            spin_lock_irqsave(&mali_pwr_lock, flags);
            if (clock_is_on(MT_CG_BG3D))
            {
                index = mt_gpufreq_get_dvfs_table_num();  // get freq. table size 
                mt_gpufreq_target(index - 1);               // set gpu to lowest freq.

                disable_clock(MT_CG_BG3D, "MFG");
                disable_clock(MT_CG_MFG_MM_SW_CG, "MFG");
            }
            
            mtk_set_input_boost_duration(0);
            
            spin_unlock_irqrestore(&mali_pwr_lock, flags);

            MALI_DEBUG_PRINT(4,("[-]MFG disable_clock \n"));

#if defined(CONFIG_MALI400_PROFILING)
            _mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
                                          MALI_PROFILING_EVENT_CHANNEL_GPU |
                                          MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE, 0, 0, 0, 0, 0);
#endif
            break;
    }
}
