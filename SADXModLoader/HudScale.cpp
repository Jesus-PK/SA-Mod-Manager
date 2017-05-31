#include "stdafx.h"

#include "SADXModLoader.h"
#include "Trampoline.h"
#include <stack>

#include "HudScale.h"

using std::stack;
using std::vector;

// TODO: title screen?
// TODO: subtitles/messages (mission screen, quit prompt), map, title cards somehow, credits

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, Uint32 attr, char zfunc_type);
static void __cdecl njDrawTriangle2D_r(NJS_POINT2COL *p, Int n, Float pri, Uint32 attr);
static void __cdecl Direct3D_DrawSprite_r(NJS_POINT2 *points);

static Trampoline njDrawSprite2D_Queue_t(0x00404660, 0x00404666, njDrawSprite2D_Queue_r);
static Trampoline njDrawTriangle2D_t(0x0077E9F0, 0x0077E9F8, njDrawTriangle2D_r);
static Trampoline Direct3D_DrawSprite_t(0x0077DE10, 0x0077DE18, Direct3D_DrawSprite_r);

#pragma region trampolines

static Trampoline* DisplayAllObjects_t;
static Trampoline* HudDisplayRingTimeLife_Check_t;
static Trampoline* HudDisplayScoreOrTimer_t;
static Trampoline* DrawStageMissionImage_t;
static Trampoline* DisplayPauseMenu_t;
static Trampoline* LifeGauge_Main_t;
static Trampoline* scaleScoreA;
static Trampoline* scaleTornadoHP;
static Trampoline* scaleTwinkleCircuitHUD;
static Trampoline* scaleFishingHit;
static Trampoline* scaleReel;
static Trampoline* scaleRod;
static Trampoline* scaleBigHud;
static Trampoline* scaleRodMeters;
static Trampoline* scaleAnimalPickup;
static Trampoline* scaleItemBoxSprite;
static Trampoline* scaleBalls;
static Trampoline* scaleCheckpointTime;
static Trampoline* scaleEmeraldRadarA;
static Trampoline* scaleEmeraldRadar_Grab;
static Trampoline* scaleEmeraldRadarB;
static Trampoline* scaleSandHillMultiplier;
static Trampoline* scaleIceCapMultiplier;
static Trampoline* scaleGammaTimeAddHud;
static Trampoline* scaleGammaTimeRemaining;
static Trampoline* scaleEmblemScreen;
static Trampoline* scaleBossName;
static Trampoline* scaleNightsCards;
static Trampoline* scaleNightsJackpot;
static Trampoline* scaleMissionStartClear;
static Trampoline* scaleMissionTimer;
static Trampoline* scaleMissionCounter;
static Trampoline* scaleTailsWinLose;
static Trampoline* scaleTailsRaceBar;
static Trampoline* scaleDemoPressStart;
static Trampoline* ChaoDX_Message_PlayerAction_Load_t;
static Trampoline* ChaoDX_Message_PlayerAction_Display_t;
static Trampoline* njDrawTextureMemList_t;
static Trampoline* DrawAVA_TITLE_BACK_t;
static Trampoline* DrawTiledBG_AVA_BACK_t;
static Trampoline* MissionCompleteScreen_Draw_t;
static Trampoline* RecapBackground_Main_t;
static Trampoline* CharSelBg_Display_t;
static Trampoline* TrialLevelList_Display_t;
static Trampoline* SubGameLevelList_Display_t;
static Trampoline* EmblemResultMenu_Display_t;
static Trampoline* FileSelect_Display_t;
static Trampoline* MenuObj_Display_t;
static Trampoline* OptionsMenu_Display_t;
static Trampoline* SoundTest_Display_t;
static Trampoline* DisplayLogoScreen_t;
static Trampoline* GreenMenuRect_Draw_t;
static Trampoline* TutorialBackground_Display_t;
static Trampoline* TutorialInstructionOverlay_Display_t;
static Trampoline* DisplayTitleCard_t;

#pragma endregion

#pragma region sprite stack

enum FillMode : Uint8
{
	Stretch = 0,
	Fit     = 1,
	Fill    = 2
};

enum Align : Uint8
{
	Auto,
	HorizontalCenter = 1 << 0,
	VerticalCenter   = 1 << 1,
	Center           = HorizontalCenter | VerticalCenter,
	Left             = 1 << 2,
	Top              = 1 << 3,
	Right            = 1 << 4,
	Bottom           = 1 << 5
};

struct ScaleEntry
{
	Uint8 alignment;
	NJS_POINT2 scale;
	bool is_background;
};

static stack<ScaleEntry, vector<ScaleEntry>> scale_stack;
static size_t stack_size = 0;

static const float patch_dummy = 1.0f;

static constexpr float third_h = 640.0f / 3.0f;
static constexpr float third_v = 480.0f / 3.0f;

// TODO: configurable
static auto fill_mode = FillMode::Fill;

static bool  do_scale  = false;
static float scale_min = 0.0f;
static float scale_max = 0.0f;
static float scale_h   = 0.0f;
static float scale_v   = 0.0f;

static NJS_POINT2 region_fit  = { 0.0f, 0.0f };
static NJS_POINT2 region_fill = { 0.0f, 0.0f };

static void __cdecl ScalePush(Uint8 align, bool is_background, float h = 1.0f, float v = 1.0f)
{
#ifdef _DEBUG
	if (ControllerPointers[0]->HeldButtons & Buttons_Z)
	{
		return;
	}
#endif

	scale_stack.push({ align, HorizontalStretch, VerticalStretch, is_background });

	HorizontalStretch = h;
	VerticalStretch = v;

	do_scale = true;
}

static void __cdecl ScalePop()
{
	if (scale_stack.size() < 1)
	{
		return;
	}

	auto point = scale_stack.top();
	HorizontalStretch = point.scale.x;
	VerticalStretch = point.scale.y;

	scale_stack.pop();
	do_scale = scale_stack.size() > 0;
}

static void __cdecl DisplayAllObjects_r()
{
	if (do_scale)
	{
		ScalePush(Align::Auto, false, scale_h, scale_v);
	}

	auto original = (decltype(DisplayAllObjects_r)*)DisplayAllObjects_t->Target();
	original();

	if (do_scale)
	{
		ScalePop();
	}
}

static NJS_POINT2 AutoAlign(Uint8 align, const NJS_POINT2& center)
{
	if (align == Align::Auto)
	{
		if (center.x < third_h)
		{
			align |= Align::Left;
		}
		else if (center.x < third_h * 2.0f)
		{
			align |= Align::HorizontalCenter;
		}
		else
		{
			align |= Align::Right;
		}

		if (center.y < third_v)
		{
			align |= Align::Top;
		}
		else if (center.y < third_v * 2.0f)
		{
			align |= Align::VerticalCenter;
		}
		else
		{
			align |= Align::Bottom;
		}
	}

	NJS_POINT2 result = {};

	if (align & Align::HorizontalCenter)
	{
		if ((float)HorizontalResolution / scale_v > 640.0f)
		{
			result.x = ((float)HorizontalResolution - region_fit.x) / 2.0f;
		}
	}
	else if (align & Align::Right)
	{
		result.x = (float)HorizontalResolution - region_fit.x;
	}

	if (align & Align::VerticalCenter)
	{
		if ((float)VerticalResolution / scale_h > 480.0f)
		{
			result.y = ((float)VerticalResolution - region_fit.y) / 2.0f;
		}
	}
	else if (align & Align::Bottom)
	{
		result.y = (float)VerticalResolution - region_fit.y;
	}

	return result;
}

static NJS_POINT2 AutoAlign(Uint8 align, const NJS_POINT3& center)
{
	return AutoAlign(align, *(NJS_POINT2*)&center);
}

/**
 * \brief Scales and re-positions an array of structures containing the fields x and y.
 * \tparam T A structure type containing the fields x and y.
 * \param points Pointer to an array of T.
 * \param count Length of the array
 */
template <typename T>
static void ScalePoints(T* points, size_t count)
{
	if (scale_stack.empty())
	{
		return;
	}

	auto top = scale_stack.top();
	auto align = top.alignment;

	NJS_POINT2 center = {};
	auto m = 1.0f / (float)count;

	for (size_t i = 0; i < count; i++)
	{
		const auto& p = points[i];
		center.x += p.x * m;
		center.y += p.y * m;
	}

	NJS_POINT2 offset;
	float scale;

	bool is_bg = !scale_stack.empty() && scale_stack.top().is_background;

	// if we're scaling a background with fill mode, manually set
	// coordinate offset so the entire image lands in the center.
	if (is_bg && fill_mode == FillMode::Fill)
	{
		scale = scale_max;
		offset.x = (HorizontalResolution - region_fill.x) / 2.0f;
		offset.y = (VerticalResolution - region_fill.y) / 2.0f;
	}
	else
	{
		scale = scale_min;
		offset = AutoAlign(align, center);
	}

	for (size_t i = 0; i < count; i++)
	{
		auto& p = points[i];
		p.x = p.x * scale + offset.x;
		p.y = p.y * scale + offset.y;
	}
}

static NJS_SPRITE last_sprite = {};
static void __cdecl SpritePush(NJS_SPRITE* sp)
{
	if (scale_stack.empty())
	{
		return;
	}

	auto top = scale_stack.top();
	auto align = top.alignment;

	auto offset = AutoAlign(align, sp->p);

	last_sprite = *sp;

	sp->p.x = sp->p.x * scale_min + offset.x;
	sp->p.y = sp->p.y * scale_min + offset.y;
	sp->sx *= scale_min;
	sp->sy *= scale_min;
}

static void __cdecl SpritePop(NJS_SPRITE* sp)
{
	sp->p = last_sprite.p;
	sp->sx = last_sprite.sx;
	sp->sy = last_sprite.sy;
}

#pragma endregion

#pragma region template garbage

/**
 * \brief Calls a trampoline function.
 * \tparam T Function type
 * \tparam Args
 * \param align Alignment mode
 * \param t Trampoline
 * \param args Option arguments for function
 */
template<typename T, typename... Args>
void ScaleTrampoline(Uint8 align, bool is_bg, const T&, const Trampoline* t, Args... args)
{
	ScalePush(align, is_bg);
	((T*)t->Target())(args...);
	ScalePop();
}

/**
 * \brief Calls a trampoline function with a return type.
 * \tparam R Return type
 * \tparam T Function type
 * \tparam Args 
 * \param align Alignment mode
 * \param t Trampoline
 * \param args Optional arguments for function
 * \return Return value of trampoline function
 */
template<typename R, typename T, typename... Args>
R ScaleTrampoline(Uint8 align, bool is_bg, const T&, const Trampoline* t, Args... args)
{
	ScalePush(align, is_bg);
	R result = ((T*)t->Target())(args...);
	ScalePop();
	return result;
}

#pragma endregion

#pragma region trampoline functions

static void __cdecl ScaleResultScreen(ObjectMaster* _this)
{
	ScalePush(Align::Center, false);
	ScoreDisplay_Main(_this);
	ScalePop();
}

static void __cdecl HudDisplayRingTimeLife_Check_r()
{
	ScaleTrampoline(Align::Auto, false, HudDisplayRingTimeLife_Check_r, HudDisplayRingTimeLife_Check_t);
}
static void __cdecl HudDisplayScoreOrTimer_r()
{
	ScaleTrampoline(Align::Left, false, HudDisplayScoreOrTimer_r, HudDisplayScoreOrTimer_t);
}

static void __cdecl DrawStageMissionImage_r(ObjectMaster* _this)
{
	ScaleTrampoline(Align::Center, false, DrawStageMissionImage_r, DrawStageMissionImage_t, _this);
}

static short __cdecl DisplayPauseMenu_r()
{
	return ScaleTrampoline<short>(Align::Center, false, DisplayPauseMenu_r, DisplayPauseMenu_t);
}

static void __cdecl LifeGauge_Main_r(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Right, false, LifeGauge_Main_r, LifeGauge_Main_t, a1);
}

static void __cdecl ScaleScoreA()
{
	ScaleTrampoline(Align::Left, false, ScaleScoreA, scaleScoreA);
}

static void __cdecl ScaleTornadoHP(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Left, false, ScaleTornadoHP, scaleTornadoHP, a1);
}

static void __cdecl ScaleTwinkleCircuitHUD(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleTwinkleCircuitHUD, scaleTwinkleCircuitHUD, a1);
}

static void __cdecl ScaleFishingHit(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleFishingHit, scaleFishingHit, a1);
}

static void __cdecl ScaleReel()
{
	ScaleTrampoline(Align::Auto, false, ScaleReel, scaleReel);
}
static void __cdecl ScaleRod()
{
	ScaleTrampoline(Align::Auto, false, ScaleRod, scaleRod);
}

static void __cdecl ScaleBigHud(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleBigHud, scaleBigHud, a1);
}
static void __cdecl ScaleRodMeters(float a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleRodMeters, scaleRodMeters, a1);
}

static void __cdecl ScaleAnimalPickup(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Right | Align::Bottom, false, ScaleAnimalPickup, scaleAnimalPickup, a1);
}

static void __cdecl ScaleItemBoxSprite(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Bottom | Align::HorizontalCenter, false, ScaleItemBoxSprite, scaleItemBoxSprite, a1);
}

static void __cdecl ScaleBalls(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Right, false, ScaleBalls, scaleBalls, a1);
}

static void __cdecl ScaleCheckpointTime(int a1, int a2, int a3)
{
	ScaleTrampoline(Align::Right | Align::Bottom, false, ScaleCheckpointTime, scaleCheckpointTime, a1, a2, a3);
}

static void __cdecl ScaleEmeraldRadarA(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleEmeraldRadarA, scaleEmeraldRadarA, a1);
}
static void __cdecl ScaleEmeraldRadar_Grab(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleEmeraldRadar_Grab, scaleEmeraldRadar_Grab, a1);
}
static void __cdecl ScaleEmeraldRadarB(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleEmeraldRadarB, scaleEmeraldRadarB, a1);
}

static void __cdecl ScaleSandHillMultiplier(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleSandHillMultiplier, scaleSandHillMultiplier, a1);
}
static void __cdecl ScaleIceCapMultiplier(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleIceCapMultiplier, scaleIceCapMultiplier, a1);
}

static void __cdecl ScaleGammaTimeAddHud(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Right, false, ScaleGammaTimeAddHud, scaleGammaTimeAddHud, a1);
}
static void __cdecl ScaleGammaTimeRemaining(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Bottom | Align::HorizontalCenter, false, ScaleGammaTimeRemaining, scaleGammaTimeRemaining, a1);
}

static void __cdecl ScaleEmblemScreen(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleEmblemScreen, scaleEmblemScreen, a1);
}

static void __cdecl ScaleBossName(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleBossName, scaleBossName, a1);
}

static void __cdecl ScaleNightsCards(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleNightsCards, scaleNightsCards, a1);
}
static void __cdecl ScaleNightsJackpot(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Auto, false, ScaleNightsJackpot, scaleNightsJackpot, a1);
}

static void __cdecl ScaleMissionStartClear(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleMissionStartClear, scaleMissionStartClear, a1);
}
static void __cdecl ScaleMissionTimer()
{

	ScaleTrampoline(Align::Center, false, ScaleMissionTimer, scaleMissionTimer);
}
static void __cdecl ScaleMissionCounter()
{
	ScaleTrampoline(Align::Center, false, ScaleMissionCounter, scaleMissionCounter);
}

static void __cdecl ScaleTailsWinLose(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, ScaleTailsWinLose, scaleTailsWinLose, a1);
}
static void __cdecl ScaleTailsRaceBar(ObjectMaster* a1)
{
	ScaleTrampoline(Align::HorizontalCenter | Align::Bottom, false, ScaleTailsRaceBar, scaleTailsRaceBar, a1);
}

static void __cdecl ScaleDemoPressStart(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Right, false, ScaleDemoPressStart, scaleDemoPressStart, a1);
}

static void __cdecl ChaoDX_Message_PlayerAction_Load_r()
{
	ScaleTrampoline(Align::Auto, false, ChaoDX_Message_PlayerAction_Load_r, ChaoDX_Message_PlayerAction_Load_t);
}

static void __cdecl ChaoDX_Message_PlayerAction_Display_r(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Top | Align::Right, false, ChaoDX_Message_PlayerAction_Display_r, ChaoDX_Message_PlayerAction_Display_t, a1);
}

static void __cdecl DrawAVA_TITLE_BACK_r(float depth)
{
	ScaleTrampoline(Align::Center, true, DrawAVA_TITLE_BACK_r, DrawAVA_TITLE_BACK_t, depth);
}

static void __cdecl DrawTiledBG_AVA_BACK_r(float depth)
{
	ScaleTrampoline(Align::Center, true, DrawTiledBG_AVA_BACK_r, DrawTiledBG_AVA_BACK_t, depth);
}

void __cdecl MissionCompleteScreen_Draw_r()
{
	ScaleTrampoline(Align::Center, false, MissionCompleteScreen_Draw_r, MissionCompleteScreen_Draw_t);
}

static void __cdecl RecapBackground_Main_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, true, RecapBackground_Main_r, RecapBackground_Main_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl CharSelBg_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, CharSelBg_Display_r, CharSelBg_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl TrialLevelList_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, TrialLevelList_Display_r, TrialLevelList_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl SubGameLevelList_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, SubGameLevelList_Display_r, SubGameLevelList_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl EmblemResultMenu_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, EmblemResultMenu_Display_r, EmblemResultMenu_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl FileSelect_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, FileSelect_Display_r, FileSelect_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl MenuObj_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, MenuObj_Display_r, MenuObj_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl OptionsMenu_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, OptionsMenu_Display_r, OptionsMenu_Display_t, a1);
}

// HACK: unconditionally scales everything
static void __cdecl SoundTest_Display_r(ObjectMaster *a1)
{
	ScaleTrampoline(Align::Center, false, SoundTest_Display_r, SoundTest_Display_t, a1);
}

static void __cdecl DisplayLogoScreen_r(Uint8 index)
{
	ScaleTrampoline(Align::Center, true, DisplayLogoScreen_r, DisplayLogoScreen_t, index);
}

static void __cdecl GreenMenuRect_Draw_r(float x, float y, float z, float width, float height)
{
	ScaleTrampoline(Align::Center, false, GreenMenuRect_Draw_r, GreenMenuRect_Draw_t, x, y, z, width, height);
}

static void __cdecl TutorialBackground_Display_r(ObjectMaster* a1)
{
	if (fill_mode == FillMode::Fill)
	{
		auto orig = fill_mode;
		fill_mode = FillMode::Fit;
		ScaleTrampoline(Align::Center, true, TutorialBackground_Display_r, TutorialBackground_Display_t, a1);
		fill_mode = orig;
	}
	else
	{
		ScaleTrampoline(Align::Center, true, TutorialBackground_Display_r, TutorialBackground_Display_t, a1);
	}
}

static void __cdecl TutorialInstructionOverlay_Display_r(ObjectMaster* a1)
{
	ScaleTrampoline(Align::Center, false, TutorialInstructionOverlay_Display_r, TutorialInstructionOverlay_Display_t, a1);
}

static Sint32 __cdecl DisplayTitleCard_r()
{
	return ScaleTrampoline<Sint32>(Align::Center, false, DisplayTitleCard_r, DisplayTitleCard_t);
}

#pragma endregion

void SetHudScaleValues()
{
	scale_h = HorizontalStretch;
	scale_v = VerticalStretch;

	scale_min = min(scale_h, scale_v);
	scale_max = max(scale_h, scale_v);

	region_fit.x  = 640.0f * scale_min;
	region_fit.y  = 480.0f * scale_min;
	region_fill.x = 640.0f * scale_max;
	region_fill.y = 480.0f * scale_max;
}

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, Uint32 attr, char zfunc_type)
{
	if (sp == nullptr)
	{
		return;
	}

	FunctionPointer(void, original, (NJS_SPRITE*, Int, Float, Uint32, char), njDrawSprite2D_Queue_t.Target());

	// Scales lens flare and sun.
	// It uses njProjectScreen so there's no position scaling required.
	if (sp == (NJS_SPRITE*)0x009BF3B0)
	{
		sp->sx *= scale_min;
		sp->sy *= scale_min;
		original(sp, n, pri, attr, zfunc_type);
	}
	else if (do_scale)
	{
		SpritePush(sp);
		original(sp, n, pri, attr | NJD_SPRITE_SCALE, zfunc_type);
		SpritePop(sp);
	}
	else
	{
		original(sp, n, pri, attr, zfunc_type);
	}
}

static void __cdecl njDrawTextureMemList_r(NJS_TEXTURE_VTX *vtx, Int count, Int tex, Int flag)
{
	auto orig = (decltype(njDrawTextureMemList_r)*)njDrawTextureMemList_t->Target();

	bool is_bg = !scale_stack.empty() && scale_stack.top().is_background;
	if (is_bg && fill_mode == FillMode::Stretch || !do_scale || count > 4)
	{
		orig(vtx, count, tex, flag);
		return;
	}

	// vtx[0] == top left
	// vtx[1] == bottom left
	// vtx[2] == top right
	// vtx[3] == bottom right

	ScalePoints(vtx, count);
	orig(vtx, count, tex, flag);
}

static void __cdecl njDrawTriangle2D_r(NJS_POINT2COL *p, Int n, Float pri, Uint32 attr)
{
	auto original = (decltype(njDrawTriangle2D_r)*)njDrawTriangle2D_t.Target();

	if (do_scale)
	{
		auto _n = n;
		if (attr & (NJD_DRAW_FAN | NJD_DRAW_CONNECTED))
		{
			_n += 2;
		}
		else
		{
			_n *= 3;
		}

		ScalePoints(p->p, _n);
	}

	original(p, n, pri, attr);
}

static void __cdecl Direct3D_DrawSprite_r(NJS_POINT2* points)
{
	auto original = (decltype(Direct3D_DrawSprite_r)*)Direct3D_DrawSprite_t.Target();

	if (do_scale)
	{
		ScalePoints(points, 8);
	}

	original(points);
}

void SetupHudScale()
{
	SetHudScaleValues();

	// This has to be created dynamically to repair a function call.
	njDrawTextureMemList_t = new Trampoline(0x0077DC70, 0x0077DC79, njDrawTextureMemList_r);
	WriteCall((void*)((size_t)njDrawTextureMemList_t->Target() + 4), njAlphaMode);

	DrawAVA_TITLE_BACK_t                 = new Trampoline(0x0050BA90, 0x0050BA96, DrawAVA_TITLE_BACK_r);
	DrawTiledBG_AVA_BACK_t               = new Trampoline(0x00507BB0, 0x00507BB5, DrawTiledBG_AVA_BACK_r);
	MissionCompleteScreen_Draw_t         = new Trampoline(0x00590690, 0x00590695, MissionCompleteScreen_Draw_r);
	RecapBackground_Main_t               = new Trampoline(0x00643C90, 0x00643C95, RecapBackground_Main_r);
	CharSelBg_Display_t                  = new Trampoline(0x00512450, 0x00512455, CharSelBg_Display_r);
	TrialLevelList_Display_t             = new Trampoline(0x0050B410, 0x0050B415, TrialLevelList_Display_r);
	SubGameLevelList_Display_t           = new Trampoline(0x0050A640, 0x0050A645, SubGameLevelList_Display_r);
	EmblemResultMenu_Display_t           = new Trampoline(0x0050DFD0, 0x0050DFD5, EmblemResultMenu_Display_r);
	FileSelect_Display_t                 = new Trampoline(0x00505550, 0x00505555, FileSelect_Display_r);
	MenuObj_Display_t                    = new Trampoline(0x00432480, 0x00432487, MenuObj_Display_r);
	OptionsMenu_Display_t                = new Trampoline(0x00509810, 0x00509815, OptionsMenu_Display_r);
	SoundTest_Display_t                  = new Trampoline(0x00511390, 0x00511395, SoundTest_Display_r);
	DisplayLogoScreen_t                  = new Trampoline(0x0042CB20, 0x0042CB28, DisplayLogoScreen_r);
	GreenMenuRect_Draw_t                 = new Trampoline(0x004334F0, 0x004334F5, GreenMenuRect_Draw_r);
	TutorialBackground_Display_t         = new Trampoline(0x006436B0, 0x006436B7, TutorialBackground_Display_r);
	TutorialInstructionOverlay_Display_t = new Trampoline(0x006430F0, 0x006430F7, TutorialInstructionOverlay_Display_r);
	DisplayTitleCard_t                   = new Trampoline(0x0047E170, 0x0047E175, DisplayTitleCard_r);

	// Fixes character scale on character select screen.
	WriteData((const float**)0x0051285E, &patch_dummy);

	WriteJump((void*)0x0042BEE0, ScaleResultScreen);

	DisplayAllObjects_t = new Trampoline(0x0040B540, 0x0040B546, DisplayAllObjects_r);
	WriteCall((void*)((size_t)DisplayAllObjects_t->Target() + 1), (void*)0x004128F0);

	HudDisplayRingTimeLife_Check_t = new Trampoline(0x00425F90, 0x00425F95, HudDisplayRingTimeLife_Check_r);
	HudDisplayScoreOrTimer_t       = new Trampoline(0x00427F50, 0x00427F55, HudDisplayScoreOrTimer_r);
	DrawStageMissionImage_t        = new Trampoline(0x00457120, 0x00457126, DrawStageMissionImage_r);

	DisplayPauseMenu_t = new Trampoline(0x00415420, 0x00415425, DisplayPauseMenu_r);
	WriteCall(DisplayPauseMenu_t->Target(), (void*)0x40FDC0);

	LifeGauge_Main_t = new Trampoline(0x004B3830, 0x004B3837, LifeGauge_Main_r);

	scaleScoreA = new Trampoline(0x00628330, 0x00628335, ScaleScoreA);

	WriteData((const float**)0x006288C2, &patch_dummy);
	scaleTornadoHP = new Trampoline(0x00628490, 0x00628496, ScaleTornadoHP);

	// TODO: Consider tracking down the individual functions so that they can be individually aligned.
	scaleTwinkleCircuitHUD = new Trampoline(0x004DB5E0, 0x004DB5E5, ScaleTwinkleCircuitHUD);
	WriteCall(scaleTwinkleCircuitHUD->Target(), (void*)0x590620);

	// Rod scaling
	scaleReel = new Trampoline(0x0046C9F0, 0x0046C9F5, ScaleReel);
	scaleRod = new Trampoline(0x0046CAB0, 0x0046CAB9, ScaleRod);
	scaleRodMeters = new Trampoline(0x0046CC70, 0x0046CC75, ScaleRodMeters);
	scaleFishingHit = new Trampoline(0x0046C920, 0x0046C926, ScaleFishingHit);
	scaleBigHud = new Trampoline(0x0046FB00, 0x0046FB05, ScaleBigHud);

	scaleAnimalPickup = new Trampoline(0x0046B330, 0x0046B335, ScaleAnimalPickup);

	scaleItemBoxSprite = new Trampoline(0x004C0790, 0x004C0795, ScaleItemBoxSprite);

	scaleBalls = new Trampoline(0x005C0B70, 0x005C0B75, ScaleBalls);

	scaleCheckpointTime = new Trampoline(0x004BABE0, 0x004BABE5, ScaleCheckpointTime);
	WriteData((const float**)0x0044F2E1, &patch_dummy);
	WriteData((const float**)0x0044F30B, &patch_dummy);
	WriteData((const float**)0x00476742, &patch_dummy);
	WriteData((const float**)0x0047676A, &patch_dummy);

	// EmeraldRadarHud_Load
	WriteData((const float**)0x00475BE3, &patch_dummy);
	WriteData((const float**)0x00475C00, &patch_dummy);
	// Emerald get
	WriteData((const float**)0x00477E8E, &patch_dummy);
	WriteData((const float**)0x00477EC0, &patch_dummy);

	scaleEmeraldRadarA = new Trampoline(0x00475A70, 0x00475A75, ScaleEmeraldRadarA);
	scaleEmeraldRadarB = new Trampoline(0x00475E50, 0x00475E55, ScaleEmeraldRadarB);
	scaleEmeraldRadar_Grab = new Trampoline(0x00475D50, 0x00475D55, ScaleEmeraldRadar_Grab);

	scaleSandHillMultiplier = new Trampoline(0x005991A0, 0x005991A6, ScaleSandHillMultiplier);
	scaleIceCapMultiplier = new Trampoline(0x004EC120, 0x004EC125, ScaleIceCapMultiplier);

	WriteData((const float**)0x0049FF70, &patch_dummy);
	WriteData((const float**)0x004A005B, &patch_dummy);
	WriteData((const float**)0x004A0067, &patch_dummy);
	scaleGammaTimeAddHud = new Trampoline(0x0049FDA0, 0x0049FDA5, ScaleGammaTimeAddHud);
	scaleGammaTimeRemaining = new Trampoline(0x004C51D0, 0x004C51D7, ScaleGammaTimeRemaining);

	WriteData((float**)0x004B4470, &scale_h);
	WriteData((float**)0x004B444E, &scale_v);
	scaleEmblemScreen = new Trampoline(0x004B4200, 0x004B4205, ScaleEmblemScreen);

	scaleBossName = new Trampoline(0x004B33D0, 0x004B33D5, ScaleBossName);

	scaleNightsCards = new Trampoline(0x005D73F0, 0x005D73F5, ScaleNightsCards);
	WriteData((float**)0x005D701B, &scale_h);
	scaleNightsJackpot = new Trampoline(0x005D6E60, 0x005D6E67, ScaleNightsJackpot);

	scaleMissionStartClear = new Trampoline(0x00591260, 0x00591268, ScaleMissionStartClear);

	scaleMissionTimer = new Trampoline(0x00592D50, 0x00592D59, ScaleMissionTimer);
	scaleMissionCounter = new Trampoline(0x00592A60, 0x00592A68, ScaleMissionCounter);

	scaleTailsWinLose = new Trampoline(0x0047C480, 0x0047C485, ScaleTailsWinLose);
	scaleTailsRaceBar = new Trampoline(0x0047C260, 0x0047C267, ScaleTailsRaceBar);

	scaleDemoPressStart = new Trampoline(0x00457D30, 0x00457D36, ScaleDemoPressStart);

#if 0
	ChaoDX_Message_PlayerAction_Load_t = new Trampoline(0x0071B3B0, 0x0071B3B7, ChaoDX_Message_PlayerAction_Load_r);
	ChaoDX_Message_PlayerAction_Display_t = new Trampoline(0x0071B210, 0x0071B215, ChaoDX_Message_PlayerAction_Display_r);
	WriteCall(ChaoDX_Message_PlayerAction_Display_t->Target(), (void*)0x590620);
#endif
}
