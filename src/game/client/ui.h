/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_H
#define GAME_CLIENT_UI_H

// TODO: Animations Rect rework
enum ANIMATION_TYPE
{
	AnimIN,
	AnimOUT,
	LINEAR,
	LINEARBACK,
	EaseIN,				// Smooth, slow at first, slow at the end, very slow overall speed
	EaseOUT,			// Smooth, fast at first, slow at the end, total speed is very slow
	EaseINOUT,			// Smoothly, at the beginning and at the end slowly, the overall speed is very slow
	EaseOUTIN,			//
	EaseIN2,			// Smooth, slow at first, slow at last, slow overall speed
	EaseOUT2,			// Smoothly, fast at first, slow at the end, the total speed is slow
	EaseINOUT2,			// Smoothly, at the beginning and at the end slowly, the total speed is slow
	EaseOUTIN2,			//
	EaseIN3,			// Smooth, slow at first, fast at last, overall speed is average
	EaseOUT3,			// Smoothly, fast at first, slow at last, overall speed is average
	EaseINOUT3,			// Smoothly, slowly at the beginning and at the end, the overall speed is average
	EaseOUTIN3,			//
	EaseIN4,			// Smooth, slow at first, fast at the end, fast overall speed - fast
	EaseOUT4,			// Smoothly, fast at the beginning, slow at the end, overall speed - fast
	EaseINOUT4,			// Smoothly, at the beginning and at the end slowly, the overall speed - fast
	EaseOUTIN4,			//
	EaseIN5,			// Smooth, slow at first, fast at last, overall speed - very fast
	EaseOUT5,			// Smooth, fast at first, slow at last, overall speed - very fast
	EaseINOUT5,			// Smoothly, at the beginning and at the end slowly, the overall speed - very fast
	EaseOUTIN5,			//
	EaseIN6,            // Smooth, slow at first, very fast at the end, overall speed - very fast
	EaseOUT6,           // Smooth, fast at first, slow at last, total speed - very fast
	EaseINOUT6,         // Smoothly, at the beginning and at the end slowly, the overall speed is very fast
	EaseOUTIN6,			//
	EaseINCirc,         // Smooth, slow at first, very fast at the end, very fast overall speed
	EaseOUTCirc,        // Smooth at first, fast at the end, slow at the end, overall speed - faster than the previous ones
	EaseINOUTCirc,      // Smoothly, at the beginning and at the end slowly, the overall speed is faster than the previous ones
	EaseOUTINCirc,		//
	EaseINBack,			// Smoothly late, late at first.
	EaseOUTBack,		// Smoothly late, late at the end.
	EaseINOUTBack,		// Smoothly late, early and late at the beginning and end.
	EaseOUTINBack,		//
	EaseINElastic,		// gently, elastically at the end
	EaseOUTElastic,		// gently, elastically at first
	EaseINOUTElastic,	// gently, in the center elastic
	EaseOUTINELastic,	//
	EaseINBounce,		// gently, jumps slowly
	EaseOUTBounce,		// gently, immediately jumps
	EaseINOUTBounce,	// smoothly, jumps both at the beginning and at the end.
	EaseOUTINBounce,	//
	Default,
};

class CUIRect
{
public:
	float x, y, w, h;

	void HSplitMid(CUIRect* pTop, CUIRect* pBottom, float Spacing = 0.0f) const;
	void HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	void HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	void VSplitMid(CUIRect* pLeft, CUIRect* pRight, float Spacing = 0.0f) const;
	void VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const;
	void VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const;

	void Margin(float Cut, CUIRect *pOtherRect) const;
	void VMargin(float Cut, CUIRect *pOtherRect) const;
	void HMargin(float Cut, CUIRect *pOtherRect) const;

	bool Inside(float x, float y) const;
};

class CUI
{
	enum
	{
		MAX_CLIP_NESTING_DEPTH = 16
	};

	bool m_Enabled;

	const void *m_pHotItem;
	const void *m_pActiveItem;
	const void *m_pLastActiveItem;
	const void *m_pBecommingHotItem;
	bool m_ActiveItemValid;
	float m_MouseX, m_MouseY; // in gui space
	float m_MouseWorldX, m_MouseWorldY; // in world space
	unsigned m_MouseButtons;
	unsigned m_LastMouseButtons;

	CUIRect m_Screen;

	CUIRect m_aClips[MAX_CLIP_NESTING_DEPTH];
	unsigned m_NumClips;
	void UpdateClipping();

	class IGraphics* m_pGraphics;
	class IInput* m_pInput;
	class ITextRender* m_pTextRender;

public:
	static const vec4 ms_DefaultTextColor;
	static const vec4 ms_DefaultTextOutlineColor;
	static const vec4 ms_HighlightTextColor;
	static const vec4 ms_HighlightTextOutlineColor;
	static const vec4 ms_TransparentTextColor;

	// TODO: Refactor: Fill this in
	void Init(class IGraphics *pGraphics, class IInput* pInput, class ITextRender *pTextRender)
	{ 
		m_pGraphics = pGraphics; 
		m_pInput = pInput;
		m_pTextRender = pTextRender;
	}
	class IGraphics* Graphics() const { return m_pGraphics; }
	class IInput* Input() const { return m_pInput; }
	class ITextRender *TextRender() const { return m_pTextRender; }

	CUI();

	enum
	{
		CORNER_TL=1,
		CORNER_TR=2,
		CORNER_BL=4,
		CORNER_BR=8,
		CORNER_ITL=16,
		CORNER_ITR=32,
		CORNER_IBL=64,
		CORNER_IBR=128,

		CORNER_T=CORNER_TL|CORNER_TR,
		CORNER_B=CORNER_BL|CORNER_BR,
		CORNER_R=CORNER_TR|CORNER_BR,
		CORNER_L=CORNER_TL|CORNER_BL,

		CORNER_IT=CORNER_ITL|CORNER_ITR,
		CORNER_IB=CORNER_IBL|CORNER_IBR,
		CORNER_IR=CORNER_ITR|CORNER_IBR,
		CORNER_IL=CORNER_ITL|CORNER_IBL,

		CORNER_ALL=CORNER_T|CORNER_B,
		CORNER_INV_ALL=CORNER_IT|CORNER_IB
	};

	enum EAlignment
	{
		ALIGN_LEFT,
		ALIGN_CENTER,
		ALIGN_RIGHT,
	};

	void SetEnabled(bool Enabled) { m_Enabled = Enabled; }
	bool Enabled() const { return m_Enabled; }
	void Update(float MouseX, float MouseY, float MouseWorldX, float MouseWorldY);

	float MouseX() const { return m_MouseX; }
	float MouseY() const { return m_MouseY; }
	float MouseWorldX() const { return m_MouseWorldX; }
	float MouseWorldY() const { return m_MouseWorldY; }
	bool MouseButton(int Index) const { return (m_MouseButtons >> Index) & 1; }
	bool MouseButtonClicked(int Index) const { return MouseButton(Index) && !((m_LastMouseButtons >> Index) & 1); }

	void SetHotItem(const void *pID) { m_pBecommingHotItem = pID; }
	void SetActiveItem(const void *pID) { m_ActiveItemValid = true; m_pActiveItem = pID; if (pID) m_pLastActiveItem = pID; }
	bool CheckActiveItem(const void *pID) { if(m_pActiveItem == pID) { m_ActiveItemValid = true; return true; } return false; };
	void ClearLastActiveItem() { m_pLastActiveItem = 0; }
	const void *HotItem() const { return m_pHotItem; }
	const void *NextHotItem() const { return m_pBecommingHotItem; }
	const void *GetActiveItem() const { return m_pActiveItem; }
	const void *LastActiveItem() const { return m_pLastActiveItem; }

	void StartCheck() { m_ActiveItemValid = false; };
	void FinishCheck() { if(!m_ActiveItemValid) SetActiveItem(0); };

	bool MouseInside(const CUIRect* pRect) const { return pRect->Inside(m_MouseX, m_MouseY); };
	bool MouseInsideClip() const { return !IsClipped() || MouseInside(ClipArea()); };
	bool MouseHovered(const CUIRect* pRect) const { return MouseInside(pRect) && MouseInsideClip(); };
	void ConvertCursorMove(float* pX, float* pY, int CursorType) const;

	bool KeyPress(int Key) const;
	bool KeyIsPressed(int Key) const;

	const CUIRect* Screen();
	float PixelSize();
	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();
	const CUIRect* ClipArea() const;
	inline bool IsClipped() const { return m_NumClips > 0; };

	bool DoButtonLogic(const void* pID, const CUIRect* pRect, int Button = 0);
	int DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY);

	void DoLabel(const CUIRect* pRect, const char* pText, float FontSize, EAlignment Align, float LineWidth = -1.0f, bool MultiLine = true);
	void DoLabelHighlighted(const CUIRect* pRect, const char* pText, const char* pHighlighted, float FontSize, const vec4& TextColor, const vec4& HighlightColor);
};

#endif
