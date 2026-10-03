#define NOMINMAX
#include <windows.h>
#include <UIAutomation.h>
#include <oleauto.h>

#include <atomic>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "accessibility.h"
#include "../src/render_protocol.h"

namespace renderer::accessibility {
namespace {
struct Item {
    std::wstring name;
    RECT bounds{};
    CONTROLTYPEID type=UIA_TextControlTypeId;
    bool selected=false;
    bool toggle=false;
    bool expanded=false;
};
struct Model {
    int runtimeId=0;
    HWND hwnd=nullptr;
    std::wstring name;
    RECT bounds{};
    std::vector<Item> items;
    int selected=-1;
    int toggleIndex=-1;
};
std::atomic<int> gRuntimeId{1};

class Root;

class Child final : public IRawElementProviderSimple, public IRawElementProviderFragment, public IExpandCollapseProvider {
public:
    Child(std::shared_ptr<Model> model, std::size_t index) : model_(std::move(model)), index_(index) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override {
        if (!value) return E_POINTER;
        *value=nullptr;
        if (iid==__uuidof(IUnknown) || iid==__uuidof(IRawElementProviderSimple))
            *value=static_cast<IRawElementProviderSimple*>(this);
        else if (iid==__uuidof(IRawElementProviderFragment))
            *value=static_cast<IRawElementProviderFragment*>(this);
        else if (iid==__uuidof(IExpandCollapseProvider) && model_->items[index_].toggle)
            *value=static_cast<IExpandCollapseProvider*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value=--refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override { if(!value)return E_POINTER;*value=ProviderOptions_ServerSideProvider|ProviderOptions_UseComThreading;return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID pattern,IUnknown** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(pattern==UIA_ExpandCollapsePatternId && model_->items[index_].toggle)return QueryInterface(__uuidof(IExpandCollapseProvider),reinterpret_cast<void**>(value));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property,VARIANT* value) override {
        if(!value)return E_POINTER;VariantInit(value);const Item& item=model_->items[index_];
        if(property==UIA_NamePropertyId){value->vt=VT_BSTR;value->bstrVal=SysAllocStringLen(item.name.data(),static_cast<UINT>(item.name.size()));return value->bstrVal?S_OK:E_OUTOFMEMORY;}
        if(property==UIA_ControlTypePropertyId){value->vt=VT_I4;value->lVal=item.type;return S_OK;}
        if(property==UIA_IsControlElementPropertyId||property==UIA_IsContentElementPropertyId||property==UIA_IsEnabledPropertyId){value->vt=VT_BOOL;value->boolVal=VARIANT_TRUE;return S_OK;}
        if(property==UIA_IsKeyboardFocusablePropertyId){value->vt=VT_BOOL;value->boolVal=VARIANT_FALSE;return S_OK;}
        if(property==UIA_IsOffscreenPropertyId){value->vt=VT_BOOL;value->boolVal=VARIANT_FALSE;return S_OK;}
        if(property==UIA_SelectionItemIsSelectedPropertyId){value->vt=VT_BOOL;value->boolVal=item.selected?VARIANT_TRUE:VARIANT_FALSE;return S_OK;}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** value) override { if(!value)return E_POINTER;*value=nullptr;return S_OK; }
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,IRawElementProviderFragment** value) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** value) override {
        if(!value)return E_POINTER;*value=SafeArrayCreateVector(VT_I4,0,3);if(!*value)return E_OUTOFMEMORY;
        LONG index=0;int parts[3]{UiaAppendRuntimeId,model_->runtimeId,static_cast<int>(index_)};
        for(int part:parts){SafeArrayPutElement(*value,&index,&part);++index;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* value) override {
        if(!value)return E_POINTER;const RECT& r=model_->items[index_].bounds;
        *value={static_cast<double>(r.left),static_cast<double>(r.top),static_cast<double>(r.right-r.left),static_cast<double>(r.bottom-r.top)};return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** value) override { if(!value)return E_POINTER;*value=SafeArrayCreateVector(VT_UNKNOWN,0,0);return *value?S_OK:E_OUTOFMEMORY; }
    HRESULT STDMETHODCALLTYPE SetFocus() override { return UIA_E_NOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** value) override;
    HRESULT STDMETHODCALLTYPE Expand() override { if(!model_->items[index_].toggle)return UIA_E_NOTSUPPORTED;return PostMessageW(model_->hwnd,WM_APP+90,1,0)?S_OK:HRESULT_FROM_WIN32(GetLastError()); }
    HRESULT STDMETHODCALLTYPE Collapse() override { if(!model_->items[index_].toggle)return UIA_E_NOTSUPPORTED;return PostMessageW(model_->hwnd,WM_APP+90,0,0)?S_OK:HRESULT_FROM_WIN32(GetLastError()); }
    HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(ExpandCollapseState* value) override {
        if(!value)return E_POINTER;if(!model_->items[index_].toggle)return UIA_E_NOTSUPPORTED;
        *value=model_->items[index_].expanded?ExpandCollapseState_Expanded:ExpandCollapseState_Collapsed;return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};std::shared_ptr<Model> model_;std::size_t index_;
};

class Root final : public IRawElementProviderSimple, public IRawElementProviderFragmentRoot, public IRawElementProviderFragment {
public:
    explicit Root(std::shared_ptr<Model> model):model_(std::move(model)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IRawElementProviderSimple))*value=static_cast<IRawElementProviderSimple*>(this);
        else if(iid==__uuidof(IRawElementProviderFragment))*value=static_cast<IRawElementProviderFragment*>(this);
        else if(iid==__uuidof(IRawElementProviderFragmentRoot))*value=static_cast<IRawElementProviderFragmentRoot*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const ULONG value=--refs_;if(!value)delete this;return value;}
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override{if(!value)return E_POINTER;*value=ProviderOptions_ServerSideProvider|ProviderOptions_UseComThreading;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID,IUnknown** value) override{if(!value)return E_POINTER;*value=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property,VARIANT* value) override {
        if(!value)return E_POINTER;VariantInit(value);
        if(property==UIA_NamePropertyId){value->vt=VT_BSTR;value->bstrVal=SysAllocStringLen(model_->name.data(),static_cast<UINT>(model_->name.size()));return value->bstrVal?S_OK:E_OUTOFMEMORY;}
        if(property==UIA_ControlTypePropertyId){value->vt=VT_I4;value->lVal=UIA_WindowControlTypeId;return S_OK;}
        if(property==UIA_IsControlElementPropertyId||property==UIA_IsContentElementPropertyId||property==UIA_IsEnabledPropertyId){value->vt=VT_BOOL;value->boolVal=VARIANT_TRUE;return S_OK;}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** value) override {if(!value)return E_POINTER;return UiaHostProviderFromHwnd(model_->hwnd,value);}
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,IRawElementProviderFragment** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(direction==NavigateDirection_FirstChild&&!model_->items.empty())*value=new(std::nothrow) Child(model_,0);
        else if(direction==NavigateDirection_LastChild&&!model_->items.empty())*value=new(std::nothrow) Child(model_,model_->items.size()-1);
        return *value?S_OK:S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** value) override {
        if(!value)return E_POINTER;*value=SafeArrayCreateVector(VT_I4,0,2);if(!*value)return E_OUTOFMEMORY;
        LONG index=0;int parts[2]{UiaAppendRuntimeId,model_->runtimeId};for(int part:parts){SafeArrayPutElement(*value,&index,&part);++index;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* value) override {if(!value)return E_POINTER;const RECT&r=model_->bounds;*value={static_cast<double>(r.left),static_cast<double>(r.top),static_cast<double>(r.right-r.left),static_cast<double>(r.bottom-r.top)};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** value) override{if(!value)return E_POINTER;*value=SafeArrayCreateVector(VT_UNKNOWN,0,0);return *value?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE SetFocus() override{return UIA_E_NOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** value) override{if(!value)return E_POINTER;*value=this;AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x,double y,IRawElementProviderFragment** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        for(std::size_t i=0;i<model_->items.size();++i){const RECT&r=model_->items[i].bounds;if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom){*value=new(std::nothrow) Child(model_,i);return *value?S_OK:E_OUTOFMEMORY;}}
        *value=this;AddRef();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** value) override {
        if(!value)return E_POINTER;*value=nullptr;return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};std::shared_ptr<Model> model_;
};

HRESULT Child::Navigate(NavigateDirection direction,IRawElementProviderFragment** value){
    if(!value)return E_POINTER;*value=nullptr;
    if(direction==NavigateDirection_Parent)*value=new(std::nothrow) Root(model_);
    else if(direction==NavigateDirection_NextSibling&&index_+1<model_->items.size())*value=new(std::nothrow) Child(model_,index_+1);
    else if(direction==NavigateDirection_PreviousSibling&&index_>0)*value=new(std::nothrow) Child(model_,index_-1);
    return *value?S_OK:S_OK;
}
HRESULT Child::get_FragmentRoot(IRawElementProviderFragmentRoot** value){if(!value)return E_POINTER;*value=new(std::nothrow) Root(model_);return *value?S_OK:E_OUTOFMEMORY;}

RECT DipBounds(const completionist::layout::DipRect& box,const completionist::render::Rect& origin,unsigned dpi){
    const double scale=static_cast<double>(dpi)/96.0;
    return {origin.left+static_cast<LONG>(box.left*scale),origin.top+static_cast<LONG>(box.top*scale),
            origin.left+static_cast<LONG>(box.right*scale),origin.top+static_cast<LONG>(box.bottom*scale)};
}
void AddItem(const std::shared_ptr<Model>& model,std::wstring name,RECT bounds,CONTROLTYPEID type,bool selected=false,bool toggle=false,bool expanded=false){
    model->items.push_back({std::move(name),bounds,type,selected,toggle,expanded});
    if(selected)model->selected=static_cast<int>(model->items.size()-1);
}
std::wstring Status(const completionist::render::Snapshot& s){
    switch(s.ai){case completionist::render::AiState::Scheduled:return L"AI scheduled";case completionist::render::AiState::Working:return L"AI working";case completionist::render::AiState::Streaming:return L"AI streaming";case completionist::render::AiState::Ready:return L"AI ready";case completionist::render::AiState::Manual:return L"Manual AI";case completionist::render::AiState::Unavailable:return L"AI unavailable";default:return L"";}
}
}

Trees CreateTrees(HWND menuWindow,HWND dockWindow,const completionist::render::Snapshot& s,
                  const completionist::layout::Layout& layout,unsigned dpi,bool dockExpanded){
    Trees trees{};
    auto menu=std::make_shared<Model>();menu->runtimeId=gRuntimeId.fetch_add(1);menu->hwnd=menuWindow;menu->name=L"Completionist suggestions";menu->bounds={layout.menuBounds.left,layout.menuBounds.top,layout.menuBounds.right,layout.menuBounds.bottom};
    for(const auto& row:layout.rowOrder){
        const bool phrase=row.kind==completionist::layout::RowKind::Phrase;
        const std::size_t index=phrase?0:row.wordIndex;
        const bool selected=phrase?s.selection==-1:s.selection==static_cast<int>(index);
        std::wstring label;
        if(phrase)label=s.phraseLead+s.phrase+L", AI";
        else if(index<s.words.size()){label=s.words[index].text;if(s.words[index].origin=="local")label+=L", Local";else if(s.words[index].origin=="learned")label+=L", Learned";}
        AddItem(menu,std::move(label),DipBounds(row.bounds,layout.menuBounds,dpi),UIA_ListItemControlTypeId,selected);
    }
    if(layout.hasStatusShelf)AddItem(menu,Status(s),DipBounds(layout.statusClip,layout.menuBounds,dpi),UIA_TextControlTypeId);
    auto dock=std::make_shared<Model>();dock->runtimeId=gRuntimeId.fetch_add(1);dock->hwnd=dockWindow;dock->name=L"Completionist information dock";dock->bounds={layout.dockBounds.left,layout.dockBounds.top,layout.dockBounds.right,layout.dockBounds.bottom};
    if(dockExpanded){
        const auto& content=layout.dockContent;
        const completionist::layout::DipRect connection{content.left,content.top,content.right,content.top+18.0f};
        const completionist::layout::DipRect tense{content.left,content.top+20.0f,content.right,content.bottom};
        AddItem(dock,s.engineConnected?L"Engine connected":L"Engine offline",DipBounds(connection,layout.dockBounds,dpi),UIA_TextControlTypeId);
        AddItem(dock,L"Tense —",DipBounds(tense,layout.dockBounds,dpi),UIA_TextControlTypeId);
    }
    dock->toggleIndex=static_cast<int>(dock->items.size());
    const LONG button=MulDiv(36,static_cast<int>(dpi),96);
    RECT toggle{dock->bounds.right-button,dockExpanded?dock->bounds.top:dock->bounds.bottom-button,
                dock->bounds.right,dockExpanded?dock->bounds.top+button:dock->bounds.bottom};
    AddItem(dock,dockExpanded?L"Minimize information dock":L"Expand information dock",toggle,UIA_ButtonControlTypeId,false,true,dockExpanded);
    trees.menu=new(std::nothrow) Root(menu);trees.dock=new(std::nothrow) Root(dock);
    if(!trees.menu||!trees.dock)Release(&trees);
    return trees;
}
void Release(Trees* trees){if(!trees)return;if(trees->menu)trees->menu->Release();if(trees->dock)trees->dock->Release();trees->menu=nullptr;trees->dock=nullptr;}
}  // namespace renderer::accessibility
