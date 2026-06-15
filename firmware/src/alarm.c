#include "alarm.h"

void Alarm_Init(AlarmState *alarm, int32_t threshold_cx100, int32_t recovery_cx100)
{
    alarm->threshold_cx100 = threshold_cx100;
    alarm->recovery_cx100 = recovery_cx100;
    alarm->active = false;
    alarm->silenced = false;
}

void Alarm_Update(AlarmState *alarm, int32_t filtered_cx100)
{
    if (!alarm->active && filtered_cx100 >= alarm->threshold_cx100) {
        alarm->active = true;
        alarm->silenced = false;
    } else if (alarm->active && filtered_cx100 <= alarm->recovery_cx100) {
        alarm->active = false;
        alarm->silenced = false;
    }
}

void Alarm_Ack(AlarmState *alarm)
{
    if (alarm->active) {
        alarm->silenced = true;
    }
}

void Alarm_SetThreshold(AlarmState *alarm, int32_t threshold_cx100)
{
    alarm->threshold_cx100 = threshold_cx100;
    alarm->recovery_cx100 = threshold_cx100 - 300;
}
