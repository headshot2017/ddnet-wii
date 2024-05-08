/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <unordered_map>
#include <math.h>

#include <base/system.h>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>

#include "input.h"

//print >>f, "int inp_key_code(const char *key_name) { int i; if (!strcmp(key_name, \"-?-\")) return -1; else for (i = 0; i < 512; i++) if (!strcmp(key_strings[i], key_name)) return i; return -1; }"

// this header is protected so you don't include it from anywere
#define KEYS_INCLUDE
#include "keynames.h"
#undef KEYS_INCLUDE

#include <ogc/pad.h>
#include <wiiuse/wpad.h>

std::unordered_map<int, int> Wiikeys = {
	{WPAD_BUTTON_1, KEY_RETURN},
	{WPAD_BUTTON_MINUS, KEY_MOUSE_WHEEL_DOWN},
	{WPAD_BUTTON_PLUS, KEY_ESCAPE},
	{WPAD_BUTTON_UP, KEY_RSHIFT},
	{WPAD_BUTTON_DOWN, KEY_LSHIFT},
	{WPAD_BUTTON_LEFT, KEY_LEFT},
	{WPAD_BUTTON_RIGHT, KEY_RIGHT},
	{WPAD_NUNCHUK_BUTTON_C, KEY_SPACE},
	{WPAD_NUNCHUK_BUTTON_Z, KEY_MOUSE_WHEEL_UP},
	// nunchuk joystick: mouse
};

std::unordered_map<int, int> WiiClassickeys = {
	{WPAD_CLASSIC_BUTTON_Y, KEY_RETURN},
	{WPAD_CLASSIC_BUTTON_UP, KEY_RSHIFT},
	{WPAD_CLASSIC_BUTTON_DOWN, KEY_LSHIFT},
	{WPAD_CLASSIC_BUTTON_LEFT, KEY_LEFT},
	{WPAD_CLASSIC_BUTTON_RIGHT, KEY_RIGHT},
	{WPAD_CLASSIC_BUTTON_ZL, KEY_MOUSE_WHEEL_UP},
	{WPAD_CLASSIC_BUTTON_ZR, KEY_MOUSE_WHEEL_DOWN},
	{WPAD_CLASSIC_BUTTON_PLUS, KEY_ESCAPE},
	{WPAD_CLASSIC_BUTTON_B, KEY_SPACE},
	// left/right movement of left joystick: KEY_a, KEY_d
	// right joystick: mouse
	// L/R shoulders: KEY_MOUSE_2, KEY_MOUSE_1
};

std::unordered_map<int, int> GCkeys = {
	{PAD_BUTTON_Y, KEY_RETURN},
	{PAD_BUTTON_UP, KEY_RSHIFT},
	{PAD_BUTTON_DOWN, KEY_LSHIFT},
	{PAD_BUTTON_LEFT, KEY_LEFT},
	{PAD_BUTTON_RIGHT, KEY_RIGHT},
	{PAD_TRIGGER_Z, KEY_MOUSE_WHEEL_DOWN},
	{PAD_BUTTON_START, KEY_ESCAPE},
	{PAD_BUTTON_B, KEY_SPACE},
	// left/right movement of left joystick: KEY_a, KEY_d
	// right joystick: mouse
	// L/R shoulders: KEY_MOUSE_2, KEY_MOUSE_1
};


void CInput::AddEvent(int Unicode, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Unicode = Unicode;
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		m_NumEvents++;
	}
}

bool CInput::LeftClick()
{
	if (m_InputMode == INPUTMODE_GAMECUBE)
		return PAD_TriggerR(0) >= 128 || PAD_ButtonsHeld(0) & PAD_BUTTON_A;
	if (m_InputMode == INPUTMODE_CLASSIC)
		return WPAD_Data(0)->exp.classic.r_shoulder >= 0.5f || WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_A;
	return WPAD_ButtonsHeld(0) & WPAD_BUTTON_A;
}

bool CInput::RightClick()
{
	if (m_InputMode == INPUTMODE_GAMECUBE)
		return PAD_TriggerL(0) >= 128;
	if (m_InputMode == INPUTMODE_CLASSIC)
		return WPAD_Data(0)->exp.classic.l_shoulder >= 0.5f;
	return WPAD_ButtonsHeld(0) & WPAD_BUTTON_B;
}

int CInput::LeftJoystickX()
{
	if (m_InputMode == INPUTMODE_GAMECUBE)
		return (PAD_StickX(0) >= 64) ? 1 : (PAD_StickX(0) <= -64) ? -1 : 0;

	float ang = 0;

	if (m_InputMode == INPUTMODE_CLASSIC && WPAD_Data(0)->exp.classic.ljs.mag >= 0.5f)
		ang = WPAD_Data(0)->exp.classic.ljs.ang;
	else if (m_InputMode == INPUTMODE_WIIMOTE && WPAD_Data(0)->exp.nunchuk.js.mag >= 0.5f)
		ang = WPAD_Data(0)->exp.nunchuk.js.ang;

	return (ang >= 45 && ang <= 135) ? 1 : (ang <= -45 && ang >= -135) ? -1 : 0;
}

CInput::CInput()
{
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	mem_zero(m_aInputState, sizeof(m_aInputState));

	m_InputCurrent = 0;
	m_InputMode = INPUTMODE_WIIMOTE;
	m_InputDispatched = false;

	m_LastRelease = 0;
	m_ReleaseDelta = -1;

	m_NumEvents = 0;

	m_VideoRestartNeeded = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();

	PAD_Init();

	WPAD_Init();
	WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
}

void CInput::MouseRelative(float *x, float *y)
{
	static float nx = 0, ny = 0;
	float Sens = ((g_Config.m_ClDyncam && g_Config.m_ClDyncamMousesens) ? g_Config.m_ClDyncamMousesens : g_Config.m_InpMousesens) / 100.0f;

	// handle gamecube controller first
	if (m_InputMode == INPUTMODE_GAMECUBE)
	{
		float joyX = PAD_SubStickX(0) / 128.f;
		float joyY = -PAD_SubStickY(0) / 128.f;

		nx = clamp(nx + (joyX*Sens), 0.f, (float)Graphics()->ScreenWidth());
		ny = clamp(ny + (joyY*Sens), 0.f, (float)Graphics()->ScreenHeight());

		*x = nx;
		*y = ny;
		return;
	}

	// then try wiimote input
	u32 type;
	int res = WPAD_Probe(0, &type);

	if (res != WPAD_ERR_NONE)
	{
		*x = 0;
		*y = 0;
		return;
	}

	WPADData* wd = WPAD_Data(0);

	if (m_InputMode == INPUTMODE_CLASSIC && type == WPAD_EXP_CLASSIC)
	{
		float joyX = cosf((wd->exp.classic.rjs.ang-90) / 180 * M_PI) * wd->exp.classic.rjs.mag;
		float joyY = sinf((wd->exp.classic.rjs.ang-90) / 180 * M_PI) * wd->exp.classic.rjs.mag;

		nx = clamp(nx + (joyX*Sens), 0.f, (float)Graphics()->ScreenWidth());
		ny = clamp(ny + (joyY*Sens), 0.f, (float)Graphics()->ScreenHeight());

		*x = nx;
		*y = ny;
		return;
	}

	*x = -20+wd->ir.x*1.25f;
	*y = -20+wd->ir.y*1.25f;
}

void CInput::MouseModeAbsolute()
{
	
}

void CInput::MouseModeRelative()
{
	
}

int CInput::MouseDoubleClick()
{
	if(m_ReleaseDelta >= 0 && m_ReleaseDelta < (time_freq() / 3))
	{
		m_LastRelease = 0;
		m_ReleaseDelta = -1;
		return 1;
	}
	return 0;
}

void CInput::ClearKeyStates()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
}

int CInput::KeyState(int Key)
{
	return m_aInputState[m_InputCurrent][Key];
}

int CInput::Update()
{
	if(m_InputDispatched)
	{
		// clear and begin count on the other one
		m_InputCurrent^=1;
		mem_zero(&m_aInputCount[m_InputCurrent], sizeof(m_aInputCount[m_InputCurrent]));
		mem_zero(&m_aInputState[m_InputCurrent], sizeof(m_aInputState[m_InputCurrent]));
		m_InputDispatched = false;
	}

	{
		WPAD_ScanPads();
		PAD_ScanPads();

		static bool lastLeftClick = false;
		static bool lastRightClick = false;
		static int lastJoyX = 0;

		int down = (m_InputMode == INPUTMODE_GAMECUBE) ? PAD_ButtonsDown(0) : WPAD_ButtonsDown(0);
		int Key;

		std::unordered_map<int, int>& chosenKeys = (m_InputMode == INPUTMODE_GAMECUBE) ? GCkeys : (m_InputMode == INPUTMODE_CLASSIC) ? WiiClassickeys : Wiikeys;

		for(std::unordered_map<int, int>::iterator it = chosenKeys.begin(); it != chosenKeys.end(); ++it)
		{
			if (!(down & it->first)) continue;
			Key = it->second;
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			m_aInputState[m_InputCurrent][Key] = 1;
			AddEvent(0, Key, IInput::FLAG_PRESS);
		}

		int up = (m_InputMode == INPUTMODE_GAMECUBE) ? PAD_ButtonsUp(0) : WPAD_ButtonsUp(0);

		for(std::unordered_map<int, int>::iterator it = chosenKeys.begin(); it != chosenKeys.end(); ++it)
		{
			if (!(up & it->first)) continue;
			Key = it->second;
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			AddEvent(0, Key, IInput::FLAG_RELEASE);

			if (Key == KEY_MOUSE_1)
			{
				m_ReleaseDelta = time_get() - m_LastRelease;
				m_LastRelease = time_get();
			}
		}

		bool click = LeftClick();
		int action = (click) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
		Key = KEY_MOUSE_1;
		if (lastLeftClick != click)
		{
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			if (click) m_aInputState[m_InputCurrent][Key] = 1;
			AddEvent(0, Key, action);

			lastLeftClick = click;
		}

		click = RightClick();
		action = (click) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
		Key = KEY_MOUSE_2;
		if (lastRightClick != click)
		{
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			if (click) m_aInputState[m_InputCurrent][Key] = 1;
			AddEvent(0, Key, action);

			lastRightClick = click;
		}

		int joyX = LeftJoystickX();
		if (lastJoyX != joyX)
		{
			// release key
			Key = (lastJoyX == -1) ? KEY_a : KEY_d;
			if (lastJoyX)
			{
				m_aInputCount[m_InputCurrent][Key].m_Presses++;
				AddEvent(0, Key, IInput::FLAG_RELEASE);
			}

			// press key
			Key = (joyX == -1) ? KEY_a : KEY_d;
			if (joyX)
			{
				m_aInputCount[m_InputCurrent][Key].m_Presses++;
				m_aInputState[m_InputCurrent][Key] = 1;
				AddEvent(0, Key, IInput::FLAG_PRESS);
			}

			lastJoyX = joyX;
		}
	}

	int lastMode = m_InputMode;

	if (WPAD_ButtonsDown(0) & WIIMOTE_BUTTON_ALL || (WPAD_Data(0)->exp.type == EXP_NUNCHUK && WPAD_Data(0)->exp.nunchuk.btns))
		m_InputMode = INPUTMODE_WIIMOTE;
	else if (WPAD_Data(0)->exp.type == EXP_CLASSIC && WPAD_Data(0)->exp.classic.btns)
		m_InputMode = INPUTMODE_CLASSIC;
	else if (PAD_ButtonsDown(0))
		m_InputMode = INPUTMODE_GAMECUBE;

	if (lastMode != m_InputMode)
		dbg_msg("wii input", "changed input mode to %d", m_InputMode);

	/*
	{
		int i;
		Uint8 *pState = SDL_GetKeyState(&i);
		if(i >= KEY_LAST)
			i = KEY_LAST-1;
		mem_copy(m_aInputState[m_InputCurrent], pState, i);
	}

	// these states must always be updated manually because they are not in the GetKeyState from SDL
	int i = SDL_GetMouseState(NULL, NULL);
	if(i&SDL_BUTTON(1)) m_aInputState[m_InputCurrent][KEY_MOUSE_1] = 1; // 1 is left
	if(i&SDL_BUTTON(3)) m_aInputState[m_InputCurrent][KEY_MOUSE_2] = 1; // 3 is right
	if(i&SDL_BUTTON(2)) m_aInputState[m_InputCurrent][KEY_MOUSE_3] = 1; // 2 is middle
	if(i&SDL_BUTTON(4)) m_aInputState[m_InputCurrent][KEY_MOUSE_4] = 1;
	if(i&SDL_BUTTON(5)) m_aInputState[m_InputCurrent][KEY_MOUSE_5] = 1;
	if(i&SDL_BUTTON(6)) m_aInputState[m_InputCurrent][KEY_MOUSE_6] = 1;
	if(i&SDL_BUTTON(7)) m_aInputState[m_InputCurrent][KEY_MOUSE_7] = 1;
	if(i&SDL_BUTTON(8)) m_aInputState[m_InputCurrent][KEY_MOUSE_8] = 1;
	if(i&SDL_BUTTON(9)) m_aInputState[m_InputCurrent][KEY_MOUSE_9] = 1;

	{
		SDL_Event Event;

		while(SDL_PollEvent(&Event))
		{
			int Key = -1;
			int Action = IInput::FLAG_PRESS;
			switch (Event.type)
			{
				// handle keys
				case SDL_KEYDOWN:
					// skip private use area of the BMP(contains the unicodes for keyboard function keys on MacOS)
					if(Event.key.keysym.unicode < 0xE000 || Event.key.keysym.unicode > 0xF8FF)	// ignore_convention
						AddEvent(Event.key.keysym.unicode, 0, 0); // ignore_convention
					Key = Event.key.keysym.sym; // ignore_convention
					break;
				case SDL_KEYUP:
					Action = IInput::FLAG_RELEASE;
					Key = Event.key.keysym.sym; // ignore_convention
					break;

				// handle mouse buttons
				case SDL_MOUSEBUTTONUP:
					Action = IInput::FLAG_RELEASE;

					if(Event.button.button == 1) // ignore_convention
					{
						m_ReleaseDelta = time_get() - m_LastRelease;
						m_LastRelease = time_get();
					}

					// fall through
				case SDL_MOUSEBUTTONDOWN:
					if(Event.button.button == SDL_BUTTON_LEFT) Key = KEY_MOUSE_1; // ignore_convention
					if(Event.button.button == SDL_BUTTON_RIGHT) Key = KEY_MOUSE_2; // ignore_convention
					if(Event.button.button == SDL_BUTTON_MIDDLE) Key = KEY_MOUSE_3; // ignore_convention
					if(Event.button.button == SDL_BUTTON_WHEELUP) Key = KEY_MOUSE_WHEEL_UP; // ignore_convention
					if(Event.button.button == SDL_BUTTON_WHEELDOWN) Key = KEY_MOUSE_WHEEL_DOWN; // ignore_convention
					if(Event.button.button == 6) Key = KEY_MOUSE_6; // ignore_convention
					if(Event.button.button == 7) Key = KEY_MOUSE_7; // ignore_convention
					if(Event.button.button == 8) Key = KEY_MOUSE_8; // ignore_convention
					if(Event.button.button == 9) Key = KEY_MOUSE_9; // ignore_convention
					break;

				// other messages
				case SDL_QUIT:
					return 1;

#if defined(__ANDROID__)
				case SDL_VIDEORESIZE:
					m_VideoRestartNeeded = 1;
					break;
#endif
			}

			//
			if(Key != -1)
			{
				m_aInputCount[m_InputCurrent][Key].m_Presses++;
				if(Action == IInput::FLAG_PRESS)
					m_aInputState[m_InputCurrent][Key] = 1;
				AddEvent(0, Key, Action);
			}

		}
	}
	*/

	return 0;
}

int CInput::VideoRestartNeeded()
{
	if( m_VideoRestartNeeded )
	{
		m_VideoRestartNeeded = 0;
		return 1;
	}
	return 0;
}

IEngineInput *CreateEngineInput() { return new CInput; }
