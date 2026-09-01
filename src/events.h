/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_EVENTS_H
#define PSXF_GUARD_EVENTS_H

#include "psx.h"
#include "fixed.h"

// EVENTS flags (Psych-compatible subset)
#define EVENTS_FLAG_VARIANT 0xFFFC

#define EVENTS_FLAG_SPEED     (1 << 2) // Change Scroll Speed
#define EVENTS_FLAG_GF        (1 << 3) // Set GF Speed
#define EVENTS_FLAG_CAMZOOM   (1 << 4) // Add Camera Zoom
#define EVENTS_FLAG_SETZOOM   (1 << 5) // Set Zoom
#define EVENTS_FLAG_ZOOMIN    (1 << 6) // zoomin
#define EVENTS_FLAG_LYRICS    (1 << 7) // Lyrics
#define EVENTS_FLAG_MULTSV    (1 << 8) // Multiplier SV (freeze note_scroll)
#define EVENTS_FLAG_MAXIMA    (1 << 9) // Maxima event (for Voltex)
#define EVENTS_FLAG_SHAKE     (1 << 10) // Screen Shake
#define EVENTS_FLAG_TRIGGER   (1 << 11) // Universal Event Trigger (All Stars)
#define EVENTS_FLAG_PLAYANIM  (1 << 12) // Play Animation
#define EVENTS_FLAG_CHAR      (1 << 13) // Change Character
#define EVENTS_FLAG_STAGE     (1 << 14) // Change Stage
#define EVENTS_FLAG_SUBTITLE  (1 << 15) // Add Subtitle
#define EVENTS_FLAG_CAMZOOMCHAIN (1 << 16) // Camera Zoom Chain
#define EVENTS_FLAG_SHAKECHAIN (1 << 17) // Screen Shake Chain
#define EVENTS_FLAG_SHOWSONG  (1 << 18) // Show Song
#define EVENTS_FLAG_HIDEHUD   (1 << 19) // Hide HUD (Ocultar HUD)

#define EVENTS_FLAG_PLAYED    (1 << 31) // Event has been already played

#define CHART_POS_END 0xFFFFFFFFFFFFFFFFULL
#define LEGACY_CHART_POS_END 0xFFFFFFFFULL

// Chart event entry (parsed from chart data)
typedef struct
{
    u64 pos;    // 1/12 steps
    u64 event;  // bitfield (variant, flags)
    u64 value1; // fixed_t when applicable
    u64 value2; // fixed_t when applicable
} ChartEvent;

typedef struct
{
	fixed_t value1, value2;
} Events;

extern Events event_speed;

void Events_Tick(void);
void Events_StartEvents(void);
void Events_ApplyShake(void);
void Events_Load(void);
void Events_DrawLyrics(void);

#endif


