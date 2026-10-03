#include "../accessibility.h"
#include "../../tests/test_harness.h"

#include <UIAutomation.h>

namespace {
std::wstring Name(IRawElementProviderSimple* provider) {
    VARIANT value{};
    if (!provider || FAILED(provider->GetPropertyValue(UIA_NamePropertyId,&value)) || value.vt!=VT_BSTR) return {};
    std::wstring result(value.bstrVal,SysStringLen(value.bstrVal));
    VariantClear(&value);
    return result;
}
}

TEST(accessibility_tree_exposes_candidate_origin_selection_and_dock_status) {
    completionist::render::Snapshot snapshot{};
    snapshot.words={{L"word","local",{}},{L"learned","learned",{}}};
    snapshot.selection=1;
    snapshot.engineConnected=true;
    completionist::layout::Layout layout{};
    layout.menuBounds={100,100,400,250};
    layout.dockBounds={100,500,290,570};
    layout.dockContent={10,6,180,64};
    layout.rowOrder={
        {completionist::layout::RowKind::Word,0,40,{10,10,180,40},{10,10,180,40}},
        {completionist::layout::RowKind::Word,1,45,{10,42,180,72},{10,42,180,72}}
    };
    auto trees=renderer::accessibility::CreateTrees(nullptr,nullptr,snapshot,layout,96,true);
    CHECK(trees.menu!=nullptr&&trees.dock!=nullptr);
    IRawElementProviderFragmentRoot* menuRoot=nullptr;
    trees.menu->QueryInterface(IID_PPV_ARGS(&menuRoot));
    IRawElementProviderFragment* menuFragment=nullptr;
    menuRoot->QueryInterface(IID_PPV_ARGS(&menuFragment));
    IRawElementProviderFragment* first=nullptr;
    CHECK(SUCCEEDED(menuFragment->Navigate(NavigateDirection_FirstChild,&first)));
    CHECK(first!=nullptr);
    IRawElementProviderSimple* firstSimple=nullptr;
    first->QueryInterface(IID_PPV_ARGS(&firstSimple));
    CHECK(Name(firstSimple)==L"word, Local");
    IRawElementProviderFragment* second=nullptr;
    first->Navigate(NavigateDirection_NextSibling,&second);
    IRawElementProviderSimple* secondSimple=nullptr;
    if(second)second->QueryInterface(IID_PPV_ARGS(&secondSimple));
    VARIANT selected{};
    if(secondSimple)secondSimple->GetPropertyValue(UIA_SelectionItemIsSelectedPropertyId,&selected);
    CHECK(Name(secondSimple)==L"learned, Learned");
    CHECK(selected.vt==VT_BOOL&&selected.boolVal==VARIANT_TRUE);
    VariantClear(&selected);

    IRawElementProviderFragmentRoot* dockRoot=nullptr;
    trees.dock->QueryInterface(IID_PPV_ARGS(&dockRoot));
    IRawElementProviderFragment* dockFragment=nullptr;
    dockRoot->QueryInterface(IID_PPV_ARGS(&dockFragment));
    IRawElementProviderFragment* dockFirst=nullptr;
    dockFragment->Navigate(NavigateDirection_FirstChild,&dockFirst);
    IRawElementProviderSimple* dockFirstSimple=nullptr;
    if(dockFirst)dockFirst->QueryInterface(IID_PPV_ARGS(&dockFirstSimple));
    CHECK(Name(dockFirstSimple)==L"Engine connected");

    if(firstSimple)firstSimple->Release();if(first)first->Release();
    if(secondSimple)secondSimple->Release();if(second)second->Release();
    if(dockFirstSimple)dockFirstSimple->Release();if(dockFirst)dockFirst->Release();
    if(menuFragment)menuFragment->Release();if(dockFragment)dockFragment->Release();
    if(menuRoot)menuRoot->Release();if(dockRoot)dockRoot->Release();
    renderer::accessibility::Release(&trees);
}

TEST(accessibility_tree_omits_collapsed_body_and_keeps_expand_action) {
    completionist::render::Snapshot snapshot{};
    snapshot.words={{L"word","local",{}}};snapshot.selection=0;snapshot.engineConnected=true;
    completionist::layout::Layout layout{};
    layout.menuBounds={0,0,330,100};layout.dockBounds={20,500,210,570};layout.dockContent={10,6,180,64};
    layout.rowOrder={{completionist::layout::RowKind::Word,0,40,{10,10,180,40},{10,10,180,40}}};
    auto trees=renderer::accessibility::CreateTrees(nullptr,nullptr,snapshot,layout,96,false);
    IRawElementProviderFragmentRoot* root=nullptr;trees.dock->QueryInterface(IID_PPV_ARGS(&root));
    IRawElementProviderFragment* rootFragment=nullptr;root->QueryInterface(IID_PPV_ARGS(&rootFragment));
    IRawElementProviderFragment* child=nullptr;
    rootFragment->Navigate(NavigateDirection_FirstChild,&child);
    IRawElementProviderSimple* simple=nullptr;if(child)child->QueryInterface(IID_PPV_ARGS(&simple));
    CHECK(Name(simple)==L"Expand information dock");
    IUnknown* pattern=nullptr;
    if(simple)simple->GetPatternProvider(UIA_ExpandCollapsePatternId,&pattern);
    CHECK(pattern!=nullptr);
    if(pattern){IExpandCollapseProvider* expand=nullptr;pattern->QueryInterface(IID_PPV_ARGS(&expand));ExpandCollapseState state{};if(expand){expand->get_ExpandCollapseState(&state);CHECK(state==ExpandCollapseState_Collapsed);expand->Release();}pattern->Release();}
    if(simple)simple->Release();if(child)child->Release();
    if(rootFragment)rootFragment->Release();
    if(root)root->Release();
    renderer::accessibility::Release(&trees);
}
