/*
 * funkinchartpak by Regan "CuckyDev" Green
 * Packs Friday Night Funkin' json formatted charts into a binary file for the PSX port
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <string>
#include <cstring>

#include "json.hpp"
using json = nlohmann::json;

#define SECTION_FLAG_OPPFOCUS (1ULL << 15) // Focus on opponent
#define SECTION_FLAG_BPM_MASK 0x7FFF       // 1/24
#define CHART_FORMAT_MAGIC 0x55435631u       // 'UCV1' marker for the new u64-capable format
#define CHART_POS_END UINT64_MAX

struct Section
{
    uint64_t end;
    uint32_t flag = 0;
};

#define NOTE_FLAG_SUSTAIN     (1 << 4) //Note is a sustain note
#define NOTE_FLAG_SUSTAIN_END (1 << 5) //Is either end of sustain
#define NOTE_FLAG_ALT_ANIM    (1 << 6) //Note plays alt animation
#define NOTE_FLAG_MINE        (1 << 7) //Note is a mine
#define NOTE_FLAG_DANGER      (1 << 8) //Note is a danger
#define NOTE_FLAG_STATIC      (1 << 9) //Note is a static
#define NOTE_FLAG_PHANTOM     (1 << 10) //Note is a phantom
#define NOTE_FLAG_POLICE      (1 << 11) //Note is a police
#define NOTE_FLAG_MAGIC       (1 << 12) //Note is a magic
#define NOTE_FLAG_HIT         (1 << 13) //Note has been hit
#define NOTE_FLAG_SLAM        (1 << 14) //Note is a slam
#define NOTE_FLAG_HALF        (1 << 15) //Note is a half slam
#define NOTE_FLAG_ASBUD       (1 << 16) //Note makes all characters (lg, w4r, y0sh) sing together
#define NOTE_FLAG_YOSHI       (1 << 17) //Note makes y0sh sing solo
#define NOTE_FLAG_GFSING      (1 << 18) //Note makes w4r sing solo, also works with mmgf  
#define NOTE_FLAG_GFDUO       (1 << 19) //Note makes gf duet sing with player/opponent
#define NOTE_FLAG_NOANIM      (1 << 20) //Note doesn't play sing animation
#define NOTE_FLAG_ASBUD       (1 << 16) //Note triggers w4r (bud note)
#define NOTE_FLAG_YOSHI       (1 << 17) //Note triggers y0sh (yoshi note)  
#define NOTE_FLAG_GFSING      (1 << 18) //Note makes gf sing
#define NOTE_FLAG_GFDUO       (1 << 19) //Note makes gf duet sing
#define NOTE_FLAG_NOANIM      (1 << 20) //Note doesn't play sing animation

struct Note
{
    uint64_t pos; // 1/12 steps
    uint32_t type;
    uint16_t is_opponent;
};

// Events (Psych Engine compatible subset)
#define EVENTS_FLAG_VARIANT 0xFFFC
#define EVENTS_FLAG_SPEED (1 << 2)
#define EVENTS_FLAG_GF (1 << 3)
#define EVENTS_FLAG_CAMZOOM (1 << 4)
#define EVENTS_FLAG_SETZOOM (1 << 5)
#define EVENTS_FLAG_ZOOMIN (1 << 6)
#define EVENTS_FLAG_LYRICS (1 << 7)
#define EVENTS_FLAG_MULTSV (1 << 8)
#define EVENTS_FLAG_MAXIMA (1 << 9)
#define EVENTS_FLAG_SHAKE (1 << 10)
#define EVENTS_FLAG_TRIGGER (1 << 11)
#define EVENTS_FLAG_PLAYANIM (1 << 12)
#define EVENTS_FLAG_CHAR (1 << 13)
#define EVENTS_FLAG_STAGE (1 << 14)
#define EVENTS_FLAG_SUBTITLE (1 << 15)
#define EVENTS_FLAG_CAMZOOMCHAIN (1 << 16)
#define EVENTS_FLAG_SHAKECHAIN (1 << 17)
#define EVENTS_FLAG_SHOWSONG (1 << 18)
#define EVENTS_FLAG_HIDEHUD (1 << 19)
#define EVENTS_FLAG_PLAYED (1 << 31)

typedef int32_t fixed_t;
#define FIXED_SHIFT (10)
#define FIXED_UNIT (1 << FIXED_SHIFT)

uint64_t PosRound(double pos, double crochet);

struct Event
{
    uint64_t pos = 0;    // 1/12 steps
    uint64_t event = 0;  // flags/variant
    uint64_t value1 = 0; // fixed 22.10
    uint64_t value2 = 0; // fixed 22.10
    std::string lyric_text;
};

struct TimingSegment
{
    double milli_base;
    uint64_t step_base;
    double step_crochet;

    TimingSegment(double milli, uint64_t step, double crochet)
        : milli_base(milli), step_base(step), step_crochet(crochet)
    {
    }
};

static std::string JsonString(const json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_number_float())
        return std::to_string(value.get<double>());
    if (value.is_number_integer())
        return std::to_string(value.get<int>());
    if (value.is_number_unsigned())
        return std::to_string(value.get<unsigned int>());
    return "";
}

static uint64_t EventPosFromMilli(double milli, const std::vector<TimingSegment>& timing)
{
    const TimingSegment *segment = &timing.front();
    for (const TimingSegment& candidate : timing)
    {
        if (candidate.milli_base > milli)
            break;
        segment = &candidate;
    }

    return (segment->step_base * 12) + PosRound((milli - segment->milli_base) * 12.0, segment->step_crochet);
}

static std::string TrimString(std::string value)
{
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
        value.pop_back();
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n'))
        value.erase(value.begin());
    return value;
}

void Events_Read(json& i, Event& event_src, std::vector<Event>& event_target)
{
    std::string event_name = JsonString(i[0]);
    std::string value1 = JsonString(i[1]);
    std::string value2 = JsonString(i[2]);

    if (event_name == "" && value1 == "zoomin")
    {
        event_name = value1;
        value1 = value2;
        value2 = "";
    }

    if (event_name == "Change Scroll Speed")
		event_src.event |= EVENTS_FLAG_SPEED;

	if (event_name == "Set GF Speed")
		event_src.event |= EVENTS_FLAG_GF;

	if (event_name == "Add Camera Zoom")
		event_src.event |= EVENTS_FLAG_CAMZOOM;

	if (event_name == "Set Zoom")
		event_src.event |= EVENTS_FLAG_SETZOOM;

	if (event_name == "zoomin")
		event_src.event |= EVENTS_FLAG_ZOOMIN;

	if (event_name == "Lyrics")
		event_src.event |= EVENTS_FLAG_LYRICS;
	
	if (event_name == "Mult SV")
		event_src.event |= EVENTS_FLAG_MULTSV;

	if (event_name == "maxima")
		event_src.event |= EVENTS_FLAG_MAXIMA;

	if (event_name == "Screen Shake")
		event_src.event |= EVENTS_FLAG_SHAKE;

	if (event_name == "Triggers Universal" || event_name == "Universal Event" || event_name == "All Stars Trigger")
		event_src.event |= EVENTS_FLAG_TRIGGER;

	if (event_name == "Play Animation" || event_name == "Character Animation")
		event_src.event |= EVENTS_FLAG_PLAYANIM;

	if (event_name == "Change Character" || event_name == "Character Change")
		event_src.event |= EVENTS_FLAG_CHAR;

	if (event_name == "Change Stage" || event_name == "Stage Change")
		event_src.event |= EVENTS_FLAG_STAGE;

	if (event_name == "Add Subtitle")
		event_src.event |= EVENTS_FLAG_SUBTITLE;

	if (event_name == "Camera Zoom Chain")
		event_src.event |= EVENTS_FLAG_CAMZOOMCHAIN;

	if (event_name == "Screen Shake Chain")
		event_src.event |= EVENTS_FLAG_SHAKECHAIN;

	if (event_name == "Show Song")
		event_src.event |= EVENTS_FLAG_SHOWSONG;

	if (event_name == "Ocultar HUD" || event_name == "Hide HUD")
		event_src.event |= EVENTS_FLAG_HIDEHUD;
	
	if (event_src.event & EVENTS_FLAG_VARIANT)
	{
		// Check if Change speed event flag is set
		if (event_src.event & EVENTS_FLAG_SPEED)
		{
			// Set default values for the first and second values if they are empty strings
		    value1 = (value1 == "") ? "1" : value1;
		    value2 = (value2 == "") ? "0" : value2;
		}

		// Check if GF event flag is set
		if (event_src.event & EVENTS_FLAG_GF)
		{
		    // Set default values for the first and second values if they are empty strings
		    value1 = (value1 == "") ? "1" : value1;
		    value2 = (value2 == "") ? "0" : value2;
		}

		// Check if CAMZOOM event flag is set
		if (event_src.event & EVENTS_FLAG_CAMZOOM)
		{
		    // Set default values for the first and second values if they are empty strings
		    value1 = (value1 == "") ? "0.015" : value1; // cam zoom
		    value2 = (value2 == "") ? "0.03" : value2; // hud zoom
		}

		if (event_src.event & EVENTS_FLAG_SETZOOM)
		{
		    value1 = (value1 == "") ? "1" : value1;
		    value2 = "0";
		}

		if (event_src.event & EVENTS_FLAG_ZOOMIN)
		{
		    value1 = (value1 == "") ? "0" : value1;
		    value2 = "0";
		}

		if (event_src.event & EVENTS_FLAG_LYRICS)
		{
			std::string lyric = value1;
			std::string color = "WHITE";
                        uint32_t font_id = 0;
                        uint32_t custom_color_hex = 0;
			size_t token_pos;
			while ((token_pos = lyric.rfind("--")) != std::string::npos)
			{
				std::string token = TrimString(lyric.substr(token_pos + 2));
				lyric = TrimString(lyric.substr(0, token_pos));
				if (token == "WHITE" || token == "RED" || token == "ORANGE" ||
                                    token == "BLUE" || token == "YELLOW" || token == "GREEN" ||
                                    token == "BROWN" || token == "BLACK" || token == "PURPLE")
					color = token;
				else if (token == "CDR")
					font_id = 0;
				else if (token == "BOLD")
					font_id = 1;
				else if (token == "ARIAL")
					font_id = 2;
				else if (token.size() == 6 || token.size() == 8)
				{
					bool is_hex = true;
					for (char c : token)
						if (!isxdigit(c)) { is_hex = false; break; }
					if (is_hex)
					{
						color = "CUSTOM";
						custom_color_hex = std::stoul(token, nullptr, 16);
						if (token.size() == 8)
							custom_color_hex &= 0xFFFFFF;
					}
				}
			}

			event_src.lyric_text = lyric;
			event_src.value1 = 0;

			if (color == "CUSTOM")
			{
				uint32_t r = (custom_color_hex >> 16) & 0xFF;
				uint32_t g = (custom_color_hex >> 8) & 0xFF;
				uint32_t b = custom_color_hex & 0xFF;
				r = (r * 128) / 255;
				g = (g * 128) / 255;
				b = (b * 128) / 255;
				event_src.value2 = 0xFF000000 | (r << 16) | (g << 8) | b;
			}
			else
			{
				uint32_t color_id = 0;
				if (color == "RED")    color_id = 1;
				else if (color == "ORANGE") color_id = 2;
				else if (color == "BLUE")   color_id = 3;
				else if (color == "YELLOW") color_id = 4;
				else if (color == "GREEN")  color_id = 5;
				else if (color == "BROWN")  color_id = 6;
				else if (color == "BLACK")  color_id = 7;
				else if (color == "PURPLE") color_id = 8;
				else if (color == "WHITE")  color_id = 9;

				uint32_t scale_id = 0;
				if (value2 != "")
					scale_id = static_cast<uint32_t>(std::stoi(value2) + 1);

				event_src.value2 = color_id | (scale_id << 8) | (font_id << 16);
			}
			std::cout << "Found event!: " << event_name << " " << lyric << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_MULTSV)
		{
			// value1: 0=stop, 1=resume (raw integer, no FIXED_UNIT)
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			event_src.value2 = 0;
			std::cout << "Found event!: " << event_name << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_MAXIMA)
		{
			uint32_t action = 1; // default rise
			if (value1 == "listen") action = 2;
			else if (value1 == "go") action = 3;
			else if (value1 == "one") action = 4;
			else if (value1 == "more") action = 5;
			else if (value1 == "set") action = 6;
			else if (value1 == "ultimate") action = 7;
			event_src.value1 = action;
			event_src.value2 = 0;
			std::cout << "Found event!: " << event_name << " " << value1 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_SHAKE)
		{
			// value1 = "duration_sec,intensity" for game camera
			// value2 = same for HUD camera
			auto parseShake = [](const std::string &val) -> uint32_t {
				if (val.empty()) return 0;
				auto comma = val.find(',');
				if (comma == std::string::npos) return 0;
				double duration = std::stod(val.substr(0, comma));
				double intensity = std::stod(val.substr(comma + 1));
				uint32_t frames = static_cast<uint32_t>(duration * 60.0 + 0.5);
				if (frames < 1 && duration > 0.0) frames = 1;
				if (frames > 0xFFFF) frames = 0xFFFF;
				uint32_t intensity_fixed = static_cast<uint32_t>(intensity * FIXED_UNIT);
				if (intensity_fixed > 0xFFFF) intensity_fixed = 0xFFFF;
				return (frames << 16) | intensity_fixed;
			};
			event_src.value1 = parseShake(value1);
			event_src.value2 = parseShake(value2);
			std::cout << "Found event!: " << event_name << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_TRIGGER)
		{
			// Universal Event Trigger (All Stars)
			// value1: act switch (0-4), value2: trigger value
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			try {
				event_src.value2 = static_cast<uint32_t>(std::stoul(value2));
			} catch (...) {
				event_src.value2 = 0;
			}
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_PLAYANIM)
		{
			// Play Animation Event
			// value1: character target (0=bf, 1=dad, 2=gf), value2: animation index
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			try {
				event_src.value2 = static_cast<uint32_t>(std::stoul(value2));
			} catch (...) {
				event_src.value2 = 0;
			}
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_CHAR)
		{
			// Change Character Event
			// value1: character slot, value2: character ID
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			try {
				event_src.value2 = static_cast<uint32_t>(std::stoul(value2));
			} catch (...) {
				event_src.value2 = 0;
			}
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_STAGE)
		{
			// Change Stage Event
			// value1: stage ID, value2: transition flags
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			try {
				event_src.value2 = static_cast<uint32_t>(std::stoul(value2));
			} catch (...) {
				event_src.value2 = 0;
			}
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_SUBTITLE)
		{
			// Add Subtitle Event - similar to lyrics but with color support
			std::string subtitle_text = value1;
			std::string color_hex = value2;
			
			event_src.lyric_text = subtitle_text;
			event_src.value1 = 0; // subtitle text stored separately
			
			// Parse hex color (0xFFRRGGBB format)
			if (!color_hex.empty() && color_hex.size() >= 8 && color_hex.substr(0, 2) == "0x")
			{
				try {
					uint32_t hex_color = std::stoul(color_hex, nullptr, 16);
					uint32_t r = (hex_color >> 16) & 0xFF;
					uint32_t g = (hex_color >> 8) & 0xFF;
					uint32_t b = hex_color & 0xFF;
					// Convert to 0-128 range for PSX
					r = (r * 128) / 255;
					g = (g * 128) / 255;
					b = (b * 128) / 255;
					event_src.value2 = 0xFF000000 | (r << 16) | (g << 8) | b;
				} catch (...) {
					event_src.value2 = 0xFF808080; // Default gray
				}
			}
			else
			{
				event_src.value2 = 0xFF808080; // Default gray
			}
			
			std::cout << "Found event!: " << event_name << " \"" << subtitle_text << "\" " << color_hex << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_CAMZOOMCHAIN)
		{
			// Camera Zoom Chain Event
			// value1: "intensity1, intensity2" for camera and hud zoom
			// value2: "count, interval" for number of zooms and timing
			auto parseZoomChain = [](const std::string &val) -> uint32_t {
				if (val.empty()) return 0;
				auto comma = val.find(',');
				if (comma == std::string::npos) return 0;
				double val1 = std::stod(val.substr(0, comma));
				double val2 = std::stod(val.substr(comma + 1));
				uint32_t param1 = static_cast<uint32_t>(val1 * FIXED_UNIT);
				uint32_t param2 = static_cast<uint32_t>(val2 * FIXED_UNIT);
				if (param1 > 0xFFFF) param1 = 0xFFFF;
				if (param2 > 0xFFFF) param2 = 0xFFFF;
				return (param1 << 16) | param2;
			};
			event_src.value1 = parseZoomChain(value1);
			event_src.value2 = parseZoomChain(value2);
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_SHAKECHAIN)
		{
			// Screen Shake Chain Event
			// value1: "duration_sec,intensity" for game camera chain
			// value2: "count,interval_sec" for number of shakes and timing
			auto parseShakeChain = [](const std::string &val) -> uint32_t {
				if (val.empty()) return 0;
				auto comma = val.find(',');
				if (comma == std::string::npos) return 0;
				double val1 = std::stod(val.substr(0, comma));
				double val2 = std::stod(val.substr(comma + 1));
				uint32_t param1, param2;
				
				// For duration,intensity format
				if (val1 < 10.0) { // Likely duration in seconds
					param1 = static_cast<uint32_t>(val1 * 60.0 + 0.5); // Convert to frames
					param2 = static_cast<uint32_t>(val2 * FIXED_UNIT); // Intensity
				} else { // For count,interval format  
					param1 = static_cast<uint32_t>(val1); // Count
					param2 = static_cast<uint32_t>(val2 * 60.0 + 0.5); // Interval in frames
				}
				
				if (param1 > 0xFFFF) param1 = 0xFFFF;
				if (param2 > 0xFFFF) param2 = 0xFFFF;
				return (param1 << 16) | param2;
			};
			event_src.value1 = parseShakeChain(value1);
			event_src.value2 = parseShakeChain(value2);
			std::cout << "Found event!: " << event_name << " " << value1 << "," << value2 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_SHOWSONG)
		{
			// Show Song Event
			// value1: show/hide flag (0=hide, 1=show)
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			event_src.value2 = 0;
			std::cout << "Found event!: " << event_name << " " << value1 << '\n';
			event_target.push_back(event_src);
			return;
		}

		if (event_src.event & EVENTS_FLAG_HIDEHUD)
		{
			// Hide HUD Event (Ocultar HUD)
			// value1: hide/show flag (0=show, 1=hide)
			try {
				event_src.value1 = static_cast<uint32_t>(std::stoul(value1));
			} catch (...) {
				event_src.value1 = 0;
			}
			event_src.value2 = 0;
			std::cout << "Found event!: " << event_name << " " << value1 << '\n';
			event_target.push_back(event_src);
			return;
		}

		//fixed values by 1024
		try {
			event_src.value1 = static_cast<uint32_t>(std::stof(value1) * FIXED_UNIT);
		} catch (...) {
			event_src.value1 = 0;
		}
		try {
			event_src.value2 = static_cast<uint32_t>(std::stof(value2) * FIXED_UNIT);
		} catch (...) {
			event_src.value2 = 0;
		}
		std::cout << "Found event!: " << event_name << '\n';

		event_target.push_back(event_src);
	}
}

uint16_t ChartKey(json &j)
{
    // Manual key count override
    if (j.contains("keys") && j["keys"].is_number_integer())
        return j["keys"].get<uint16_t>();

    // Shaggy keys preset
    switch (j.value("mania", 0))
    {
    case 0:
        return 4;
    case 1:
        return 6;
    case 2:
        return 7;
    case 3:
        return 9;
    case 4:
        return 5;
    }

    // FNF original keys
    return 4;
}

uint64_t PosRound(double pos, double crochet)
{
    return static_cast<uint64_t>(std::floor(pos / crochet + 0.5));
}

void WriteWord(std::ostream &out, uint16_t word)
{
    out.put(word >> 0);
    out.put(word >> 8);
}

void WriteDWord(std::ostream &out, uint32_t dword)
{
    out.put(static_cast<char>(dword >> 0));
    out.put(static_cast<char>(dword >> 8));
    out.put(static_cast<char>(dword >> 16));
    out.put(static_cast<char>(dword >> 24));
}

void WriteQWord(std::ostream &out, uint64_t qword)
{
    out.put(static_cast<char>(qword >> 0));
    out.put(static_cast<char>(qword >> 8));
    out.put(static_cast<char>(qword >> 16));
    out.put(static_cast<char>(qword >> 24));
    out.put(static_cast<char>(qword >> 32));
    out.put(static_cast<char>(qword >> 40));
    out.put(static_cast<char>(qword >> 48));
    out.put(static_cast<char>(qword >> 56));
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: funkinchartpak in_json" << std::endl;
        return 0;
    }

    // Read json
    std::ifstream i(argv[1]);
    if (!i.is_open())
    {
        std::cout << "Failed to open " << argv[1] << std::endl;
        return 1;
    }
    json j;
    i >> j;

    auto song_info = j["song"];

    double bpm = song_info["bpm"];
    double crochet = (60.0 / bpm) * 1000.0;
    double step_crochet = crochet / 4;

    double speed = song_info.value("speed", 0.0);
    uint16_t keys = ChartKey(song_info);
    uint16_t max_keys = keys * 2;
    uint8_t lanes = song_info.value("lanes", 2);

    std::cout << argv[1] << " speed: " << speed << " ini bpm: " << bpm << " step_crochet: " << step_crochet << " keys: " << keys << std::endl;

    uint64_t milli_base = 0;
    uint64_t step_base = 0;

    std::vector<Section> sections;
    std::vector<Note> notes;
    std::vector<Event> events;

    uint64_t section_end = 0;
    std::vector<TimingSegment> timing;
    timing.push_back({0.0, 0, step_crochet});
    int score = 0, dups = 0;
    for (auto &i : song_info["notes"]) // Iterate through sections
    {
        bool is_opponent = i["mustHitSection"] != true; // Note: swapped

        // Read section
        Section new_section;
        if (i["changeBPM"] == true)
        {
            // Update BPM (THIS IS HELL!)
            milli_base += step_crochet * (section_end - step_base);
            step_base = section_end;

            bpm = i["bpm"];
            crochet = (60.0 / bpm) * 1000.0;
            step_crochet = crochet / 4;
            timing.push_back({(double)milli_base, step_base, step_crochet});

            std::cout << "chg bpm: " << bpm << " step_crochet: " << step_crochet << " milli_base: " << milli_base << " step_base: " << step_base << std::endl;
        }
        new_section.end = (section_end += 16) * 12; //(uint32_t)i["lengthInSteps"]) * 12; // I had to do this for compatibility
        new_section.flag = PosRound(bpm, 1.0 / 24.0) & SECTION_FLAG_BPM_MASK;
        bool is_alt = i["altAnim"] == true;
        if (is_opponent)
            new_section.flag |= SECTION_FLAG_OPPFOCUS;
        sections.push_back(new_section);

        // Read notes
        for (auto &j : i["sectionNotes"])
        {
            // Push main note
            Note new_note;
			
			//Event type
			if (j[1] == -1)
				continue;
			
            int sustain = static_cast<int>(PosRound(j[2], step_crochet)) - 1;
            new_note.pos = (step_base * 12) + PosRound(((uint32_t)j[0] - milli_base) * 12.0, step_crochet);
            new_note.type = static_cast<uint32_t>(j[1]) % max_keys;

            // Remap FX notes to the FX lane (lane 4 in 7K voltex)
            if (j[3] == "FX")
                new_note.type = 4;

            new_note.is_opponent = false;

            if (lanes > 1)
            {
                if ((!is_opponent && new_note.type >= keys) || (is_opponent && new_note.type < keys))
                    new_note.is_opponent = true;

                if (is_opponent)
                    new_note.type = (new_note.type + keys) % max_keys;
            }

            if (j[3] == "Alt Animation")
                new_note.type |= NOTE_FLAG_ALT_ANIM;

            else if ((new_note.is_opponent) && is_alt)
                new_note.type |= NOTE_FLAG_ALT_ANIM;

            if (sustain >= 0)
                new_note.type |= NOTE_FLAG_SUSTAIN_END;

            if (j[3] == "Hurt Note")
                new_note.type |= NOTE_FLAG_MINE;

            if (j[3] == "HD Note")
                new_note.type |= NOTE_FLAG_DANGER;

            if (j[3] == "Static Note")
                new_note.type |= NOTE_FLAG_STATIC;

            if (j[3] == "Phantom Note")
                new_note.type |= NOTE_FLAG_PHANTOM;

            if (j[3] == "police")
                new_note.type |= NOTE_FLAG_POLICE;

            if (j[3] == "magic")
                new_note.type |= NOTE_FLAG_MAGIC;

            if (j[3] == "Half Slam Lane 1" || j[3] == "Half Slam Lane 2" || j[3] == "Half Slam Lane 3")
                new_note.type |= NOTE_FLAG_HALF;

            if (j[3] == "Normal Slam")
                new_note.type |= NOTE_FLAG_SLAM;

            if (j[3] == "Bud Note" || j[3] == "AS Bud" || j[3] == "AS Bud Note" || j[3] == "asbud")
                new_note.type |= NOTE_FLAG_ASBUD;

            if (j[3] == "Yoshi Note" || j[3] == "yoshi")
                new_note.type |= NOTE_FLAG_YOSHI;

            if (j[3] == "GF Sing" || j[3] == "gfsing")
                new_note.type |= NOTE_FLAG_GFSING;

            if (j[3] == "GF Duo" || j[3] == "GF Duet" || j[3] == "gfduo")
                new_note.type |= NOTE_FLAG_GFDUO;

            if (j[3] == "No Anim" || j[3] == "No Animation" || j[3] == "noanim")
                new_note.type |= NOTE_FLAG_NOANIM;
			
            notes.push_back(new_note);
            if (!new_note.is_opponent)
                score += 350;

            // Push sustain notes
            for (int k = 0; k <= sustain; k++)
            {
                Note sus_note;
                sus_note.pos = new_note.pos + ((k + 1) * 12);
                sus_note.type = new_note.type | NOTE_FLAG_SUSTAIN;
                sus_note.is_opponent = new_note.is_opponent;

                if (k != sustain)
                    sus_note.type &= ~NOTE_FLAG_SUSTAIN_END;
                notes.push_back(sus_note);
            }
        }
    }
    std::cout << "max score: " << score << " dups excluded: " << dups << std::endl;

    // Sort notes
    std::sort(notes.begin(), notes.end(), [](Note a, Note b) {
        if (a.pos == b.pos)
            return (b.type & NOTE_FLAG_SUSTAIN) && !(a.type & NOTE_FLAG_SUSTAIN);
        else
            return a.pos < b.pos;
    });

    // Read Events from JSON (Psych Engine format)
    for (auto &i : song_info["events"]) //Iterate through sections
	{
		for (auto &j : i[1])
		{
			//Push main event
			Event new_event;

			new_event.pos = EventPosFromMilli((double)i[0], timing);
			//Newer psych engine events version
			Events_Read(j, new_event, events);
		}
	}


    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.pos != b.pos)
            return a.pos < b.pos;
        return a.event < b.event;
    });

    // Push dummy section and note
    Section dum_section;
    dum_section.end = CHART_POS_END;
    dum_section.flag = sections[sections.size() - 1].flag;
    sections.push_back(dum_section);

    Note dum_note;
    dum_note.pos = CHART_POS_END;
    dum_note.type = NOTE_FLAG_HIT;
    dum_note.is_opponent = false;
    notes.push_back(dum_note);

    Event dum_event;
    dum_event.pos = CHART_POS_END;
    dum_event.event = EVENTS_FLAG_PLAYED;
    dum_event.value1 = 0;
    dum_event.value2 = 0;
    events.push_back(dum_event);

    // Write to output
    std::ofstream out(std::string(argv[1]) + ".cht", std::ostream::binary);
    if (!out.is_open())
    {
        std::cout << "Failed to open " << argv[1] << ".cht" << std::endl;
        return 1;
    }

    // Write headers (u32 speed_fixed, u16 keys, u8 lanes, u32 note offset, u32 magic)
    // For the new u64-capable format, sections are 12 bytes, notes are 14 bytes and events are 32 bytes.
    WriteDWord(out, static_cast<fixed_t>(speed * FIXED_UNIT));
    WriteWord(out, keys);
    out.put(lanes);
    WriteDWord(out, 15 + static_cast<uint32_t>(sections.size()) * 12);
    WriteDWord(out, CHART_FORMAT_MAGIC);

    // Write sections (u64 end, u32 flag)
    for (auto &i : sections)
    {
        WriteQWord(out, i.end);
        WriteDWord(out, i.flag);
    }

    // Write notes (u64 pos, u32 type, u16 is_opponent)
    for (auto &i : notes)
    {
        WriteQWord(out, i.pos);
        WriteDWord(out, i.type);
        WriteWord(out, static_cast<uint16_t>(i.is_opponent ? 1 : 0));
    }

    uint64_t lyric_offset = 15 + static_cast<uint64_t>(sections.size()) * 12 + static_cast<uint64_t>(notes.size()) * 14 + static_cast<uint64_t>(events.size()) * 32;

    // Write events (u64 pos, u64 event, u64 value1, u64 value2)
    for (auto &e : events)
    {
        WriteQWord(out, e.pos);
        WriteQWord(out, e.event);
        if ((e.event & EVENTS_FLAG_VARIANT) == EVENTS_FLAG_LYRICS && !e.lyric_text.empty())
        {
            WriteQWord(out, lyric_offset);
            lyric_offset += static_cast<uint64_t>(e.lyric_text.size() + 1);
        }
        else
        {
            WriteQWord(out, e.value1);
        }
        WriteQWord(out, e.value2);
    }

    for (auto &e : events)
    {
        if ((e.event & EVENTS_FLAG_VARIANT) == EVENTS_FLAG_LYRICS && !e.lyric_text.empty())
            out.write(e.lyric_text.c_str(), e.lyric_text.size() + 1);
    }
    return 0;
}
