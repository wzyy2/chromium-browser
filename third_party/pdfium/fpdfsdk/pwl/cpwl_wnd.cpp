// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pwl/cpwl_wnd.h"

#include <map>
#include <sstream>
#include <vector>

#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/pwl/cpwl_scroll_bar.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

constexpr float kDefaultFontSize = 9.0f;

}  // namespace

CPWL_Wnd::CreateParams::CreateParams()
    : rcRectWnd(0, 0, 0, 0),
      pSystemHandler(nullptr),
      pFontMap(nullptr),
      pProvider(nullptr),
      pFocusHandler(nullptr),
      dwFlags(0),
      sBackgroundColor(),
      pAttachedWidget(nullptr),
      nBorderStyle(BorderStyle::SOLID),
      dwBorderWidth(1),
      sBorderColor(),
      sTextColor(),
      nTransparency(255),
      fFontSize(kDefaultFontSize),
      sDash(3, 0, 0),
      pAttachedData(nullptr),
      pParentWnd(nullptr),
      pMsgControl(nullptr),
      eCursorType(FXCT_ARROW) {}

CPWL_Wnd::CreateParams::CreateParams(const CreateParams& other) = default;

CPWL_Wnd::CreateParams::~CreateParams() = default;

class CPWL_MsgControl : public Observable<CPWL_MsgControl> {
  friend class CPWL_Wnd;

 public:
  explicit CPWL_MsgControl(CPWL_Wnd* pWnd) {
    m_pCreatedWnd = pWnd;
    Default();
  }

  ~CPWL_MsgControl() { Default(); }

  void Default() {
    m_aMousePath.clear();
    m_aKeyboardPath.clear();
    m_pMainMouseWnd = nullptr;
    m_pMainKeyboardWnd = nullptr;
  }

  bool IsWndCreated(const CPWL_Wnd* pWnd) const {
    return m_pCreatedWnd == pWnd;
  }

  bool IsMainCaptureMouse(const CPWL_Wnd* pWnd) const {
    return pWnd == m_pMainMouseWnd;
  }

  bool IsWndCaptureMouse(const CPWL_Wnd* pWnd) const {
    return pWnd && pdfium::ContainsValue(m_aMousePath, pWnd);
  }

  bool IsMainCaptureKeyboard(const CPWL_Wnd* pWnd) const {
    return pWnd == m_pMainKeyboardWnd;
  }

  bool IsWndCaptureKeyboard(const CPWL_Wnd* pWnd) const {
    return pWnd && pdfium::ContainsValue(m_aKeyboardPath, pWnd);
  }

  void SetFocus(CPWL_Wnd* pWnd) {
    m_aKeyboardPath.clear();
    if (!pWnd)
      return;

    m_pMainKeyboardWnd = pWnd;
    CPWL_Wnd* pParent = pWnd;
    while (pParent) {
      m_aKeyboardPath.push_back(pParent);
      pParent = pParent->GetParentWindow();
    }
    // Note, pWnd may get destroyed in the OnSetFocus call.
    pWnd->OnSetFocus();
  }

  void KillFocus() {
    ObservedPtr observed_ptr(this);
    if (!m_aKeyboardPath.empty())
      if (CPWL_Wnd* pWnd = m_aKeyboardPath[0])
        pWnd->OnKillFocus();
    if (!observed_ptr)
      return;

    m_pMainKeyboardWnd = nullptr;
    m_aKeyboardPath.clear();
  }

  void SetCapture(CPWL_Wnd* pWnd) {
    m_aMousePath.clear();
    if (pWnd) {
      m_pMainMouseWnd = pWnd;
      CPWL_Wnd* pParent = pWnd;
      while (pParent) {
        m_aMousePath.push_back(pParent);
        pParent = pParent->GetParentWindow();
      }
    }
  }

  void ReleaseCapture() {
    m_pMainMouseWnd = nullptr;
    m_aMousePath.clear();
  }

 private:
  std::vector<CPWL_Wnd*> m_aMousePath;
  std::vector<CPWL_Wnd*> m_aKeyboardPath;
  UnownedPtr<CPWL_Wnd> m_pCreatedWnd;
  UnownedPtr<CPWL_Wnd> m_pMainMouseWnd;
  UnownedPtr<CPWL_Wnd> m_pMainKeyboardWnd;
};

CPWL_Wnd::CPWL_Wnd()
    : m_rcWindow(),
      m_rcClip(),
      m_bCreated(false),
      m_bVisible(false),
      m_bNotifying(false),
      m_bEnabled(true) {}

CPWL_Wnd::~CPWL_Wnd() {
  ASSERT(!m_bCreated);
}

ByteString CPWL_Wnd::GetClassName() const {
  return "CPWL_Wnd";
}

void CPWL_Wnd::Create(const CreateParams& cp) {
  if (IsValid())
    return;

  m_CreationParams = cp;
  OnCreate(&m_CreationParams);
  m_CreationParams.rcRectWnd.Normalize();
  m_rcWindow = m_CreationParams.rcRectWnd;
  m_rcClip = m_rcWindow;
  if (!m_rcClip.IsEmpty()) {
    m_rcClip.Inflate(1.0f, 1.0f);
    m_rcClip.Normalize();
  }
  CreateMsgControl();
  if (m_CreationParams.pParentWnd)
    m_CreationParams.pParentWnd->AddChild(this);

  CreateParams ccp = m_CreationParams;
  ccp.dwFlags &= 0xFFFF0000L;  // remove sub styles
  CreateScrollBar(ccp);
  CreateChildWnd(ccp);
  m_bVisible = HasFlag(PWS_VISIBLE);
  OnCreated();
  if (!RePosChildWnd())
    return;

  m_bCreated = true;
}

void CPWL_Wnd::OnCreate(CreateParams* pParamsToAdjust) {}

void CPWL_Wnd::OnCreated() {}

void CPWL_Wnd::OnDestroy() {}

void CPWL_Wnd::InvalidateFocusHandler(FocusHandlerIface* handler) {
  if (m_CreationParams.pFocusHandler == handler)
    m_CreationParams.pFocusHandler = nullptr;
}

void CPWL_Wnd::InvalidateProvider(ProviderIface* provider) {
  if (m_CreationParams.pProvider.Get() == provider)
    m_CreationParams.pProvider.Reset();
}

void CPWL_Wnd::Destroy() {
  KillFocus();
  OnDestroy();
  if (m_bCreated) {
    m_pVScrollBar = nullptr;
    for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
      CPWL_Wnd* pChild = *it;
      if (pChild) {
        *it = nullptr;
        pChild->Destroy();
        delete pChild;
      }
    }
    if (m_CreationParams.pParentWnd)
      m_CreationParams.pParentWnd->RemoveChild(this);

    m_bCreated = false;
  }
  DestroyMsgControl();
  m_Children.clear();
}

bool CPWL_Wnd::Move(const CFX_FloatRect& rcNew, bool bReset, bool bRefresh) {
  if (!IsValid())
    return true;

  CFX_FloatRect rcOld = GetWindowRect();
  m_rcWindow = rcNew;
  m_rcWindow.Normalize();

  if (bReset) {
    if (rcOld.left != rcNew.left || rcOld.right != rcNew.right ||
        rcOld.top != rcNew.top || rcOld.bottom != rcNew.bottom) {
      if (!RePosChildWnd())
        return false;
    }
  }
  if (bRefresh && !InvalidateRectMove(rcOld, rcNew))
    return false;

  m_CreationParams.rcRectWnd = m_rcWindow;
  return true;
}

bool CPWL_Wnd::InvalidateRectMove(const CFX_FloatRect& rcOld,
                                  const CFX_FloatRect& rcNew) {
  CFX_FloatRect rcUnion = rcOld;
  rcUnion.Union(rcNew);

  return InvalidateRect(&rcUnion);
}

void CPWL_Wnd::DrawAppearance(CFX_RenderDevice* pDevice,
                              const CFX_Matrix& mtUser2Device) {
  if (IsValid() && IsVisible()) {
    DrawThisAppearance(pDevice, mtUser2Device);
    DrawChildAppearance(pDevice, mtUser2Device);
  }
}

void CPWL_Wnd::DrawThisAppearance(CFX_RenderDevice* pDevice,
                                  const CFX_Matrix& mtUser2Device) {
  CFX_FloatRect rectWnd = GetWindowRect();
  if (rectWnd.IsEmpty())
    return;

  if (HasFlag(PWS_BACKGROUND)) {
    float width = static_cast<float>(GetBorderWidth() + GetInnerBorderWidth());
    pDevice->DrawFillRect(&mtUser2Device, rectWnd.GetDeflated(width, width),
                          GetBackgroundColor(), GetTransparency());
  }

  if (HasFlag(PWS_BORDER)) {
    pDevice->DrawBorder(&mtUser2Device, rectWnd,
                        static_cast<float>(GetBorderWidth()), GetBorderColor(),
                        GetBorderLeftTopColor(GetBorderStyle()),
                        GetBorderRightBottomColor(GetBorderStyle()),
                        GetBorderStyle(), GetTransparency());
  }
}

void CPWL_Wnd::DrawChildAppearance(CFX_RenderDevice* pDevice,
                                   const CFX_Matrix& mtUser2Device) {
  for (CPWL_Wnd* pChild : m_Children) {
    if (!pChild)
      continue;

    CFX_Matrix mt = pChild->GetChildMatrix();
    if (mt.IsIdentity()) {
      pChild->DrawAppearance(pDevice, mtUser2Device);
    } else {
      mt.Concat(mtUser2Device);
      pChild->DrawAppearance(pDevice, mt);
    }
  }
}

bool CPWL_Wnd::InvalidateRect(CFX_FloatRect* pRect) {
  ObservedPtr thisObserved(this);
  if (!IsValid())
    return true;

  CFX_FloatRect rcRefresh = pRect ? *pRect : GetWindowRect();

  if (!HasFlag(PWS_NOREFRESHCLIP)) {
    CFX_FloatRect rcClip = GetClipRect();
    if (!rcClip.IsEmpty()) {
      rcRefresh.Intersect(rcClip);
    }
  }

  CFX_FloatRect rcWin = PWLtoWnd(rcRefresh);
  rcWin.Inflate(1, 1);
  rcWin.Normalize();

  if (CFX_SystemHandler* pSH = GetSystemHandler()) {
    if (CPDFSDK_Widget* widget = static_cast<CPDFSDK_Widget*>(
            m_CreationParams.pAttachedWidget.Get())) {
      pSH->InvalidateRect(widget, rcWin);
      if (!thisObserved)
        return false;
    }
  }

  return true;
}

#define PWL_IMPLEMENT_KEY_METHOD(key_method_name)                  \
  bool CPWL_Wnd::key_method_name(uint16_t nChar, uint32_t nFlag) { \
    if (!IsValid() || !IsVisible() || !IsEnabled())                \
      return false;                                                \
    if (!IsWndCaptureKeyboard(this))                               \
      return false;                                                \
    for (auto* pChild : m_Children) {                              \
      if (pChild && IsWndCaptureKeyboard(pChild))                  \
        return pChild->key_method_name(nChar, nFlag);              \
    }                                                              \
    return false;                                                  \
  }

PWL_IMPLEMENT_KEY_METHOD(OnKeyDown)
PWL_IMPLEMENT_KEY_METHOD(OnChar)
#undef PWL_IMPLEMENT_KEY_METHOD

#define PWL_IMPLEMENT_MOUSE_METHOD(mouse_method_name)                          \
  bool CPWL_Wnd::mouse_method_name(const CFX_PointF& point, uint32_t nFlag) {  \
    if (!IsValid() || !IsVisible() || !IsEnabled())                            \
      return false;                                                            \
    if (IsWndCaptureMouse(this)) {                                             \
      for (auto* pChild : m_Children) {                                        \
        if (pChild && IsWndCaptureMouse(pChild)) {                             \
          return pChild->mouse_method_name(pChild->ParentToChild(point),       \
                                           nFlag);                             \
        }                                                                      \
      }                                                                        \
      SetCursor();                                                             \
      return false;                                                            \
    }                                                                          \
    for (auto* pChild : m_Children) {                                          \
      if (pChild && pChild->WndHitTest(pChild->ParentToChild(point))) {        \
        return pChild->mouse_method_name(pChild->ParentToChild(point), nFlag); \
      }                                                                        \
    }                                                                          \
    if (WndHitTest(point))                                                     \
      SetCursor();                                                             \
    return false;                                                              \
  }

PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonDblClk)
PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonDown)
PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonUp)
PWL_IMPLEMENT_MOUSE_METHOD(OnRButtonDown)
PWL_IMPLEMENT_MOUSE_METHOD(OnRButtonUp)
PWL_IMPLEMENT_MOUSE_METHOD(OnMouseMove)
#undef PWL_IMPLEMENT_MOUSE_METHOD

WideString CPWL_Wnd::GetSelectedText() {
  return WideString();
}

void CPWL_Wnd::ReplaceSelection(const WideString& text) {}

bool CPWL_Wnd::OnMouseWheel(short zDelta,
                            const CFX_PointF& point,
                            uint32_t nFlag) {
  if (!IsValid() || !IsVisible() || !IsEnabled())
    return false;

  SetCursor();
  if (!IsWndCaptureKeyboard(this))
    return false;

  for (auto* pChild : m_Children) {
    if (pChild && IsWndCaptureKeyboard(pChild))
      return pChild->OnMouseWheel(zDelta, pChild->ParentToChild(point), nFlag);
  }
  return false;
}

void CPWL_Wnd::AddChild(CPWL_Wnd* pWnd) {
  m_Children.push_back(pWnd);
}

void CPWL_Wnd::RemoveChild(CPWL_Wnd* pWnd) {
  for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
    if (*it && *it == pWnd) {
      m_Children.erase(std::next(it).base());
      break;
    }
  }
}

void CPWL_Wnd::SetScrollInfo(const PWL_SCROLL_INFO& info) {}

void CPWL_Wnd::SetScrollPosition(float pos) {}

void CPWL_Wnd::ScrollWindowVertically(float pos) {}

void CPWL_Wnd::NotifyLButtonDown(CPWL_Wnd* child, const CFX_PointF& pos) {}

void CPWL_Wnd::NotifyLButtonUp(CPWL_Wnd* child, const CFX_PointF& pos) {}

void CPWL_Wnd::NotifyMouseMove(CPWL_Wnd* child, const CFX_PointF& pos) {}

CPWL_Wnd* CPWL_Wnd::GetParentWindow() const {
  return m_CreationParams.pParentWnd;
}

CFX_FloatRect CPWL_Wnd::GetWindowRect() const {
  return m_rcWindow;
}

CFX_FloatRect CPWL_Wnd::GetClientRect() const {
  CFX_FloatRect rcWindow = GetWindowRect();

  float width = static_cast<float>(GetBorderWidth() + GetInnerBorderWidth());
  CFX_FloatRect rcClient = rcWindow.GetDeflated(width, width);
  if (CPWL_ScrollBar* pVSB = GetVScrollBar())
    rcClient.right -= pVSB->GetScrollBarWidth();

  rcClient.Normalize();
  return rcWindow.Contains(rcClient) ? rcClient : CFX_FloatRect();
}

CFX_PointF CPWL_Wnd::GetCenterPoint() const {
  CFX_FloatRect rcClient = GetClientRect();
  return CFX_PointF((rcClient.left + rcClient.right) * 0.5f,
                    (rcClient.top + rcClient.bottom) * 0.5f);
}

bool CPWL_Wnd::HasFlag(uint32_t dwFlags) const {
  return (m_CreationParams.dwFlags & dwFlags) != 0;
}

void CPWL_Wnd::RemoveFlag(uint32_t dwFlags) {
  m_CreationParams.dwFlags &= ~dwFlags;
}

void CPWL_Wnd::AddFlag(uint32_t dwFlags) {
  m_CreationParams.dwFlags |= dwFlags;
}

CFX_Color CPWL_Wnd::GetBackgroundColor() const {
  return m_CreationParams.sBackgroundColor;
}

void CPWL_Wnd::SetBackgroundColor(const CFX_Color& color) {
  m_CreationParams.sBackgroundColor = color;
}

CFX_Color CPWL_Wnd::GetTextColor() const {
  return m_CreationParams.sTextColor;
}

BorderStyle CPWL_Wnd::GetBorderStyle() const {
  return m_CreationParams.nBorderStyle;
}

void CPWL_Wnd::SetBorderStyle(BorderStyle nBorderStyle) {
  if (HasFlag(PWS_BORDER))
    m_CreationParams.nBorderStyle = nBorderStyle;
}

int32_t CPWL_Wnd::GetBorderWidth() const {
  return HasFlag(PWS_BORDER) ? m_CreationParams.dwBorderWidth : 0;
}

int32_t CPWL_Wnd::GetInnerBorderWidth() const {
  return 0;
}

CFX_Color CPWL_Wnd::GetBorderColor() const {
  return HasFlag(PWS_BORDER) ? m_CreationParams.sBorderColor : CFX_Color();
}

const CPWL_Dash& CPWL_Wnd::GetBorderDash() const {
  return m_CreationParams.sDash;
}

CPWL_Wnd::PrivateData* CPWL_Wnd::GetAttachedData() const {
  return m_CreationParams.pAttachedData.Get();
}

CPWL_ScrollBar* CPWL_Wnd::GetVScrollBar() const {
  return HasFlag(PWS_VSCROLL) ? m_pVScrollBar.Get() : nullptr;
}

void CPWL_Wnd::CreateScrollBar(const CreateParams& cp) {
  CreateVScrollBar(cp);
}

void CPWL_Wnd::CreateVScrollBar(const CreateParams& cp) {
  if (m_pVScrollBar || !HasFlag(PWS_VSCROLL))
    return;

  CreateParams scp = cp;

  // flags
  scp.dwFlags =
      PWS_CHILD | PWS_BACKGROUND | PWS_AUTOTRANSPARENT | PWS_NOREFRESHCLIP;

  scp.pParentWnd = this;
  scp.sBackgroundColor = PWL_DEFAULT_WHITECOLOR;
  scp.eCursorType = FXCT_ARROW;
  scp.nTransparency = PWL_SCROLLBAR_TRANSPARENCY;

  m_pVScrollBar = new CPWL_ScrollBar(SBT_VSCROLL);
  m_pVScrollBar->Create(scp);
}

void CPWL_Wnd::SetCapture() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl())
    pMsgCtrl->SetCapture(this);
}

void CPWL_Wnd::ReleaseCapture() {
  for (auto* pChild : m_Children) {
    if (pChild)
      pChild->ReleaseCapture();
  }
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl())
    pMsgCtrl->ReleaseCapture();
}

void CPWL_Wnd::SetFocus() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl()) {
    if (!pMsgCtrl->IsMainCaptureKeyboard(this))
      pMsgCtrl->KillFocus();
    pMsgCtrl->SetFocus(this);
  }
}

void CPWL_Wnd::KillFocus() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl()) {
    if (pMsgCtrl->IsWndCaptureKeyboard(this))
      pMsgCtrl->KillFocus();
  }
}

void CPWL_Wnd::OnSetFocus() {}

void CPWL_Wnd::OnKillFocus() {}

bool CPWL_Wnd::WndHitTest(const CFX_PointF& point) const {
  return IsValid() && IsVisible() && GetWindowRect().Contains(point);
}

bool CPWL_Wnd::ClientHitTest(const CFX_PointF& point) const {
  return IsValid() && IsVisible() && GetClientRect().Contains(point);
}

const CPWL_Wnd* CPWL_Wnd::GetRootWnd() const {
  auto* pParent = m_CreationParams.pParentWnd;
  return pParent ? pParent->GetRootWnd() : this;
}

bool CPWL_Wnd::SetVisible(bool bVisible) {
  if (!IsValid())
    return true;

  ObservedPtr thisObserved(this);

  for (auto* pChild : m_Children) {
    if (pChild) {
      pChild->SetVisible(bVisible);
      if (!thisObserved)
        return false;
    }
  }

  if (bVisible != m_bVisible) {
    m_bVisible = bVisible;
    if (!RePosChildWnd())
      return false;

    if (!InvalidateRect(nullptr))
      return false;
  }
  return true;
}

void CPWL_Wnd::SetClipRect(const CFX_FloatRect& rect) {
  m_rcClip = rect;
  m_rcClip.Normalize();
}

const CFX_FloatRect& CPWL_Wnd::GetClipRect() const {
  return m_rcClip;
}

bool CPWL_Wnd::IsReadOnly() const {
  return HasFlag(PWS_READONLY);
}

bool CPWL_Wnd::RePosChildWnd() {
  CPWL_ScrollBar* pVSB = GetVScrollBar();
  if (!pVSB)
    return true;

  CFX_FloatRect rcContent = GetWindowRect();
  if (!rcContent.IsEmpty()) {
    float width = static_cast<float>(GetBorderWidth() + GetInnerBorderWidth());
    rcContent.Deflate(width, width);
    rcContent.Normalize();
  }
  CFX_FloatRect rcVScroll =
      CFX_FloatRect(rcContent.right - PWL_SCROLLBAR_WIDTH, rcContent.bottom,
                    rcContent.right - 1.0f, rcContent.top);

  ObservedPtr thisObserved(this);

  pVSB->Move(rcVScroll, true, false);
  if (!thisObserved)
    return false;

  return true;
}

void CPWL_Wnd::CreateChildWnd(const CreateParams& cp) {}

void CPWL_Wnd::SetCursor() {
  if (IsValid()) {
    if (CFX_SystemHandler* pSH = GetSystemHandler()) {
      int32_t nCursorType = GetCreationParams().eCursorType;
      pSH->SetCursor(nCursorType);
    }
  }
}

void CPWL_Wnd::CreateMsgControl() {
  if (!m_CreationParams.pMsgControl)
    m_CreationParams.pMsgControl = new CPWL_MsgControl(this);
}

void CPWL_Wnd::DestroyMsgControl() {
  CPWL_MsgControl* pMsgControl = GetMsgControl();
  if (pMsgControl && pMsgControl->IsWndCreated(this))
    delete pMsgControl;
}

CPWL_MsgControl* CPWL_Wnd::GetMsgControl() const {
  return m_CreationParams.pMsgControl;
}

bool CPWL_Wnd::IsCaptureMouse() const {
  return IsWndCaptureMouse(this);
}

bool CPWL_Wnd::IsWndCaptureMouse(const CPWL_Wnd* pWnd) const {
  CPWL_MsgControl* pCtrl = GetMsgControl();
  return pCtrl ? pCtrl->IsWndCaptureMouse(pWnd) : false;
}

bool CPWL_Wnd::IsWndCaptureKeyboard(const CPWL_Wnd* pWnd) const {
  CPWL_MsgControl* pCtrl = GetMsgControl();
  return pCtrl ? pCtrl->IsWndCaptureKeyboard(pWnd) : false;
}

bool CPWL_Wnd::IsFocused() const {
  CPWL_MsgControl* pCtrl = GetMsgControl();
  return pCtrl ? pCtrl->IsMainCaptureKeyboard(this) : false;
}

CFX_FloatRect CPWL_Wnd::GetFocusRect() const {
  CFX_FloatRect rect = GetWindowRect();
  if (!rect.IsEmpty()) {
    rect.Inflate(1.0f, 1.0f);
    rect.Normalize();
  }
  return rect;
}

float CPWL_Wnd::GetFontSize() const {
  return m_CreationParams.fFontSize;
}

void CPWL_Wnd::SetFontSize(float fFontSize) {
  m_CreationParams.fFontSize = fFontSize;
}

CFX_SystemHandler* CPWL_Wnd::GetSystemHandler() const {
  return m_CreationParams.pSystemHandler;
}

CPWL_Wnd::FocusHandlerIface* CPWL_Wnd::GetFocusHandler() const {
  return m_CreationParams.pFocusHandler.Get();
}

CPWL_Wnd::ProviderIface* CPWL_Wnd::GetProvider() const {
  return m_CreationParams.pProvider.Get();
}

IPVT_FontMap* CPWL_Wnd::GetFontMap() const {
  return m_CreationParams.pFontMap;
}

CFX_Color CPWL_Wnd::GetBorderLeftTopColor(BorderStyle nBorderStyle) const {
  switch (nBorderStyle) {
    case BorderStyle::BEVELED:
      return CFX_Color(COLORTYPE_GRAY, 1);
    case BorderStyle::INSET:
      return CFX_Color(COLORTYPE_GRAY, 0.5f);
    default:
      return CFX_Color();
  }
}

CFX_Color CPWL_Wnd::GetBorderRightBottomColor(BorderStyle nBorderStyle) const {
  switch (nBorderStyle) {
    case BorderStyle::BEVELED:
      return GetBackgroundColor() / 2.0f;
    case BorderStyle::INSET:
      return CFX_Color(COLORTYPE_GRAY, 0.75f);
    default:
      return CFX_Color();
  }
}

int32_t CPWL_Wnd::GetTransparency() {
  return m_CreationParams.nTransparency;
}

void CPWL_Wnd::SetTransparency(int32_t nTransparency) {
  for (auto* pChild : m_Children) {
    if (pChild)
      pChild->SetTransparency(nTransparency);
  }
  m_CreationParams.nTransparency = nTransparency;
}

CFX_Matrix CPWL_Wnd::GetWindowMatrix() const {
  CFX_Matrix mt = GetChildToRoot();
  if (ProviderIface* pProvider = GetProvider())
    mt.Concat(pProvider->GetWindowMatrix(GetAttachedData()));
  return mt;
}

CFX_FloatRect CPWL_Wnd::PWLtoWnd(const CFX_FloatRect& rect) const {
  CFX_Matrix mt = GetWindowMatrix();
  return mt.TransformRect(rect);
}

CFX_PointF CPWL_Wnd::ParentToChild(const CFX_PointF& point) const {
  CFX_Matrix mt = GetChildMatrix();
  if (mt.IsIdentity())
    return point;

  CFX_Matrix inverse = mt.GetInverse();
  if (!inverse.IsIdentity())
    mt = inverse;
  return mt.Transform(point);
}

CFX_FloatRect CPWL_Wnd::ParentToChild(const CFX_FloatRect& rect) const {
  CFX_Matrix mt = GetChildMatrix();
  if (mt.IsIdentity())
    return rect;

  CFX_Matrix inverse = mt.GetInverse();
  if (!inverse.IsIdentity())
    mt = inverse;

  return mt.TransformRect(rect);
}

CFX_Matrix CPWL_Wnd::GetChildToRoot() const {
  CFX_Matrix mt;
  if (HasFlag(PWS_CHILD)) {
    const CPWL_Wnd* pParent = this;
    while (pParent) {
      mt.Concat(pParent->GetChildMatrix());
      pParent = pParent->GetParentWindow();
    }
  }
  return mt;
}

CFX_Matrix CPWL_Wnd::GetChildMatrix() const {
  return HasFlag(PWS_CHILD) ? m_CreationParams.mtChild : CFX_Matrix();
}

void CPWL_Wnd::SetChildMatrix(const CFX_Matrix& mt) {
  m_CreationParams.mtChild = mt;
}

const CPWL_Wnd* CPWL_Wnd::GetFocused() const {
  CPWL_MsgControl* pMsgCtrl = GetMsgControl();
  return pMsgCtrl ? pMsgCtrl->m_pMainKeyboardWnd.Get() : nullptr;
}

void CPWL_Wnd::EnableWindow(bool bEnable) {
  if (m_bEnabled == bEnable)
    return;

  for (auto* pChild : m_Children) {
    if (pChild)
      pChild->EnableWindow(bEnable);
  }
  m_bEnabled = bEnable;
}