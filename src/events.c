/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "events.h"
#include "stage.h"
#include "timer.h"
#include "random.h"
#include "mutil.h"
#include "font_cdrmap.h"

Events event_speed;
static fixed_t event_speed_start_time;
static fixed_t event_speed_start_speed;

static const char *event_lyric_text;
static fixed_t event_lyric_end;
static u32 event_lyric_color;
static u8 event_lyric_scale;
static u8 event_lyric_font;

// Shake event state
static fixed_t shake_intensity;
static u16 shake_duration;
static fixed_t shake_hud_intensity;
static u16 shake_hud_duration;

static void Events_ApplyStageZoom(fixed_t zoom)
{
    event.zoom = zoom;

    Character *focus = NULL;
    if (stage.cur_section != NULL && (stage.cur_section->flag & SECTION_FLAG_OPPFOCUS))
        focus = stage.opponent;
    else
        focus = stage.player;

    if (focus != NULL)
        stage.camera.tz = FIXED_MUL(focus->focus_zoom, event.zoom);
    else
        stage.camera.tz = event.zoom;
}

static void Events_Check(ChartEvent* event)
{
    switch (event->event & EVENTS_FLAG_VARIANT)
    {
        case EVENTS_FLAG_SPEED:
            event_speed.value1 = (fixed_t)event->value1;
            event_speed.value2 = (fixed_t)event->value2;
            event_speed_start_time = timer_sec;
            event_speed_start_speed = stage.speed;
            break;
        case EVENTS_FLAG_GF:
            stage.gf_speed = (u8)((event->value1 >> FIXED_SHIFT) * 4);
            break;
        case EVENTS_FLAG_CAMZOOM:
            stage.bump += (fixed_t)event->value1;
            stage.sbump += (fixed_t)event->value2;
            break;
        case EVENTS_FLAG_SETZOOM:
            Events_ApplyStageZoom((fixed_t)event->value1);
            break;
        case EVENTS_FLAG_ZOOMIN:
            Events_ApplyStageZoom(FIXED_UNIT + (fixed_t)event->value1);
            break;
        case EVENTS_FLAG_LYRICS:
        {
            event_lyric_text = (const char*)event->value1;
            event_lyric_end = timer_sec + FIXED_DEC(5,1);
            u32 val2 = event->value2;
            if ((val2 >> 24) == 0xFF)
            {
                event_lyric_color = val2;
                event_lyric_scale = 1;
                event_lyric_font = 0;
            }
            else
            {
                event_lyric_color = val2 & 0xFF;
                event_lyric_scale = (val2 >> 8) & 0xFF;
                event_lyric_font = (val2 >> 16) & 0xFF;
            }
            break;
        }
        case EVENTS_FLAG_MULTSV:
            if (event->value1 == 0) // stop
                stage.scroll_freeze_until = 0x7FFFFFFF; // freeze indefinitely
            else // resume
                stage.scroll_freeze_until = 0;
            break;
		case EVENTS_FLAG_MAXIMA:
			stage.maxima_action = (u8)(event->value1 & 0xFF);
			break;
		case EVENTS_FLAG_SHAKE:
		{
			// value1: high 16 bits = game duration (frames), low 16 bits = game intensity (fixed_t)
			shake_intensity = (fixed_t)(event->value1 & 0xFFFF);
			shake_duration = (u16)(event->value1 >> 16);
			// value2: same for HUD
			shake_hud_intensity = (fixed_t)(event->value2 & 0xFFFF);
			shake_hud_duration = (u16)(event->value2 >> 16);
			break;
		}
        default:
            break;
    }
}

static s32 Events_CDRWidth(const char *text, u8 scale)
{
    s32 width = 0;
    u8 c;

    while ((c = *text++) != '\0')
    {
        if ((c -= 0x20) >= 0x60)
            continue;
        width += (font_cdrmap[c].charW - 1) * scale;
    }

    return width;
}

static void Events_DrawCDRScaled(const char *text, fixed_t x, fixed_t y, FontAlign align, u8 r, u8 g, u8 b, u8 scale)
{
    s32 alignoffset = Events_CDRWidth(text, scale);
    u8 c;

    if (align == FontAlign_Left)
        alignoffset = 0;
    else if (align == FontAlign_Center)
        alignoffset /= 2;

    while ((c = *text++) != '\0')
    {
        if ((c -= 0x20) >= 0x60)
            continue;

        RECT src = {font_cdrmap[c].charX, 129 + font_cdrmap[c].charY, font_cdrmap[c].charW, font_cdrmap[c].charL};
        RECT_FIXED dst = {
            x - FIXED_DEC(alignoffset,1),
            y,
            (src.w * scale) << FIXED_SHIFT,
            (src.h * scale) << FIXED_SHIFT
        };
        Stage_DrawTexCol(&stage.font_cdr.tex, &src, &dst, stage.bump, stage.camera.hudangle, r, g, b);
        x += (font_cdrmap[c].charW - 1) * scale << FIXED_SHIFT;
    }
}

void Events_DrawLyrics(void)
{
    u8 r = 0x80;
    u8 g = 0x80;
    u8 b = 0x80;
    u8 scale;
    FontData *font;

    if (event_lyric_text == NULL || event_lyric_text[0] == '\0' || timer_sec >= event_lyric_end)
        return;

    if (event_lyric_color & 0xFF000000)
    {
        r = (event_lyric_color >> 16) & 0xFF;
        g = (event_lyric_color >> 8) & 0xFF;
        b = event_lyric_color & 0xFF;
    }
    else switch (event_lyric_color)
    {
        case 1:
            r = 0x80; g = 0x20; b = 0x20; break;
        case 2:
            r = 0x80; g = 0x50; b = 0x18; break;
        case 3:
            r = 0x00; g = 0x00; b = 0x80; break;
        case 4:
            r = 0x80; g = 0x80; b = 0x00; break;
        case 5:
            r = 0x00; g = 0x80; b = 0x00; break;
        case 6:
            r = 0x60; g = 0x40; b = 0x20; break;
        case 7:
            r = 0x20; g = 0x20; b = 0x20; break;
        case 8:
            r = 0x60; g = 0x40; b = 0x60; break;
        case 9:
            r = 0x80; g = 0x80; b = 0x80; break;
    }

    scale = (event_lyric_scale == 0) ? 1 : (event_lyric_scale + 1);
    switch (event_lyric_font)
    {
        case 1:
            font = &stage.font_bold;
            break;
        case 2:
            font = &stage.font_arial;
            break;
        case 0:
        default:
            font = &stage.font_cdr;
            break;
    }

    if (font == &stage.font_cdr && scale > 1)
        Events_DrawCDRScaled(event_lyric_text, FIXED_DEC(0,1), FIXED_DEC(75,1), FontAlign_Center, r, g, b, scale);
	else if (font == &stage.font_cdr)
		font->draw_col(font, event_lyric_text, FIXED_DEC(0,1), FIXED_DEC(75,1), FontAlign_Center, r, g, b);
	else
		font->draw_col(font, event_lyric_text, SCREEN_WIDTH2, SCREEN_HEIGHT2 + 75, FontAlign_Center, r, g, b);
}

void Events_Tick(void)
{
    if (event_speed.value2 > 0)
    {
        fixed_t elapsed = timer_sec - event_speed_start_time;
        if (elapsed >= event_speed.value2)
        {
            stage.speed = FIXED_MUL(stage.ogspeed, event_speed.value1);
            event_speed.value2 = 0;
        }
        else
        {
            fixed_t t = FIXED_DIV(elapsed, event_speed.value2);
            fixed_t target = FIXED_MUL(stage.ogspeed, event_speed.value1);
            stage.speed = event_speed_start_speed + FIXED_MUL(target - event_speed_start_speed, t);
        }
    }
}

void Events_StartEvents(void)
{
    u64 event_pos = (stage.song_step < 0) ? 0 : (u64)stage.song_step * 12;

    for (ChartEvent *event = stage.cur_event; event != NULL && event < stage.events_end && event->pos != CHART_POS_END; event++)
    {
        if (event->pos > event_pos)
            break;
        else
            stage.cur_event++;

        if (event->event & EVENTS_FLAG_PLAYED)
            continue;

        Events_Check(event);
        event->event |= EVENTS_FLAG_PLAYED;
    }

    for (ChartEvent *event = stage.event_cur_event; event != NULL && event < stage.event_events_end && event->pos != CHART_POS_END; event++)
    {
        if (event->pos > event_pos)
            break;
        else
            stage.event_cur_event++;

        if (event->event & EVENTS_FLAG_PLAYED)
            continue;

        Events_Check(event);
        event->event |= EVENTS_FLAG_PLAYED;
    }

    Events_Tick();
}

void Events_ApplyShake(void)
{
    if (shake_duration > 0)
    {
        stage.camera.x += RandomRange(-shake_intensity, shake_intensity) * (FIXED_UNIT / 10);
        stage.camera.y += RandomRange(-shake_intensity, shake_intensity) * (FIXED_UNIT / 10);
        shake_duration--;
    }
    if (shake_hud_duration > 0)
    {
        stage.noteshakex = (s16)(RandomRange(-shake_hud_intensity, shake_hud_intensity) * (FIXED_UNIT / 10));
        stage.noteshakey = (s16)(RandomRange(-shake_hud_intensity, shake_hud_intensity) * (FIXED_UNIT / 10));
        shake_hud_duration--;
    }
    else
    {
        stage.noteshakex = 0;
        stage.noteshakey = 0;
    }
}

void Events_Load(void)
{
    event_speed.value1 = FIXED_UNIT;
    event_speed.value2 = 0;
    event_speed_start_time = 0;
    event_speed_start_speed = 0;
    event_lyric_text = NULL;
    event_lyric_end = 0;
    event_lyric_color = 0;
    event_lyric_scale = 0;
    event_lyric_font = 0;
    shake_intensity = 0;
    shake_duration = 0;
    shake_hud_intensity = 0;
    shake_hud_duration = 0;
}


