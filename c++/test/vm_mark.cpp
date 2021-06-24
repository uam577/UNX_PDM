// This file implements the marker view

#include "cmRecord.h"
#include "vwMain.h"
#include "vwConst.h"
#include "vwConfig.h"


#ifndef PL_GROUP_MARKER
#define PL_GROUP_MARKER 0
#endif

// @#LIMITATION Markers outside a scope cannot be searched


bsString
vwMain::Marker::getDescr(void) const
{
    char tmpStr[128];
    snprintf(tmpStr, sizeof(tmpStr), "marker %d", syncMode);
    return tmpStr;
}


void
vwMain::addMarker(int id, s64 startTimeNs)
{
    _markers.push_back( { id } );
    _markers.back().forceTimeNs = startTimeNs;
    setFullScreenView(-1);
    plMarker("user", "Add a marker view");
}


void
vwMain::prepareMarker(Marker& mkr)
{
    // Check if the cache is still valid
    const double winHeight = ImGui::GetWindowSize().y; // Approximated and bigger anyway
    if(!mkr.isCacheDirty && winHeight<=mkr.lastWinHeight) return;

    // Worth working
    plgScope(MARKER, "prepareMarker");
    mkr.lastWinHeight = winHeight;
    mkr.isCacheDirty  = false;
    mkr.cachedItems.clear();

    // Precomputations on record (could be done once)
    mkr.maxIdx = _record->markerEventQty;
    // Category max length
    mkr.maxCategoryLength = 8; // For the header word "Category"
    for(int catNameIdx : _record->markerCategories) {
        int length = _record->getString(catNameIdx).value.size();
        if(length>mkr.maxCategoryLength) mkr.maxCategoryLength = length;
    }
    // Thread name max length
    mkr.maxThreadNameLength = 6; // For the header word "Thread"
    for(int i=0; i<_record->threads.size(); ++i) {
        int length = strlen(getFullThreadName(i));
        if(length>mkr.maxThreadNameLength) mkr.maxThreadNameLength = length;
    }

    cmRecord::Evt evt;
    bool isCoarseScope = false;
    // Resynchonization on a date?
    if(mkr.forceTimeNs>=0) {
        cmRecordIteratorMarker itMarker(_record, -1, -1, mkr.forceTimeNs, 0.);
        // Iterator points somewhere before the date, we need to adjust it
        while(itMarker.getNextMarker(isCoarseScope, evt) && evt.vS64<mkr.forceTimeNs) ;
        // Store the adjusted start index
        mkr.startIdx = bsMax(0, itMarker.getIndex()-1); // -1 because getNextMarker() increments
        mkr.forceTimeNs = -1;
    }

    // Collect the lines
    int  maxLineQty = 1+winHeight/ImGui::GetTextLineHeightWithSpacing();
    cmRecordIteratorMarker itMarker(_record, mkr.startIdx);
    while(mkr.cachedItems.size()<maxLineQty && itMarker.getNextMarker(isCoarseScope, evt)) {
        // Filter here on category and threads
        int catId = _record->getString(evt.nameIdx).categoryId;
        plAssert(catId>=0);
        if(mkr.isFiltered(evt.threadId, catId)) continue;
        // Find the elemIdx
        u64  itemHashPath = bsHashStepChain(_record->threads[evt.threadId].threadHash, evt.nameIdx, cmConst::MARKER_NAMEIDX);
        int* elemIdx      = _record->elemPathToId.find(itemHashPath, cmConst::MARKER_NAMEIDX);
        // Store
        mkr.cachedItems.push_back({ evt, elemIdx? *elemIdx : -1});
    }
}


void
vwMain::drawMarkers(void)
{
    if(!_record || _markers.empty()) return;
    plScope("drawMarkers");
    int itemToRemoveIdx = -1;

    for(int markerIdx=0; markerIdx<_markers.size(); ++markerIdx) {
        auto& marker = _markers[markerIdx];
        if(_uniqueIdFullScreen>=0 && marker.uniqueId!=_uniqueIdFullScreen) continue;

        // Display complete tabs
        char tmpStr[256];
        snprintf(tmpStr, sizeof(tmpStr), "Markers###%d", marker.uniqueId);
        bool isOpen = true;

        if(marker.isWindowSelected) {
            marker.isWindowSelected = false;
            ImGui::SetNextWindowFocus();
        }
        if(marker.isNew) {
            marker.isNew = false;
            if(marker.newDockId!=0xFFFFFFFF) ImGui::SetNextWindowDockID(marker.newDockId);
            else selectBestDockLocation(false, true);
        }
        if(ImGui::Begin(tmpStr, &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing)) {
            drawMarker(marker);
        }

        // End the window and cleaning
        if(!isOpen) itemToRemoveIdx = markerIdx;
        ImGui::End();
    } // End of loop on markers

    // Remove profile if needed
    if(itemToRemoveIdx>=0) {
        releaseId((_markers.begin()+itemToRemoveIdx)->uniqueId);
        _markers.erase(_markers.begin()+itemToRemoveIdx);
        dirty();
        setFullScreenView(-1);
    }
}


void
vwMain::drawMarker(Marker& mkr)
{
    plgScope(MARKER, "drawMarker");

    // Display the thread name
    double fontHeight    = ImGui::GetTextLineHeightWithSpacing();
    double textPixMargin = ImGui::GetStyle().ItemSpacing.x;
    float  charWidth     = ImGui::CalcTextSize("0").x;
    double comboWidth    = ImGui::CalcTextSize("Isolated XXX").x;
    float  textBgY       = ImGui::GetWindowPos().y+ImGui::GetCursorPos().y;
    float  comboX        = ImGui::GetWindowContentRegionMax().x-comboWidth;
    DRAWLIST->AddRectFilled(ImVec2(ImGui::GetWindowPos().x+ImGui::GetCursorPos().x-2., textBgY),
                            ImVec2(ImGui::GetWindowPos().x+comboX, textBgY+ImGui::GetTextLineHeightWithSpacing()+ImGui::GetStyle().FramePadding.y), vwConst::uGrey48);

    // Filtering menu
    // Sanity
    while(mkr.threadSelection  .size()<_record->threads         .size()) mkr.threadSelection  .push_back(true);
    while(mkr.categorySelection.size()<_record->markerCategories.size()) mkr.categorySelection.push_back(true);
    // Thread filtering
    float padMenuX    = ImGui::GetStyle().FramePadding.x;
    float offsetMenuX = ImGui::GetStyle().ItemSpacing.x+padMenuX+charWidth*14;
    float widthMenu   = ImGui::CalcTextSize("Thread").x;
    ImU32 filterBg    = ImColor(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    DRAWLIST->AddRectFilled(ImVec2(ImGui::GetWindowPos().x+offsetMenuX-padMenuX, textBgY),
                            ImVec2(ImGui::GetWindowPos().x+offsetMenuX+widthMenu+padMenuX, textBgY+ImGui::GetTextLineHeightWithSpacing()), filterBg);
    ImGui::SetCursorPosX(offsetMenuX);
    ImGui::AlignTextToFramePadding();
    if(mkr.isFilteredOnThread) ImGui::PushStyleColor(ImGuiCol_Text, vwConst::gold);
    if(ImGui::Selectable("Thread", false, 0, ImVec2(widthMenu, 0))) ImGui::OpenPopup("Thread marker menu");
    if(mkr.isFilteredOnThread) ImGui::PopStyleColor();
    if(ImGui::BeginPopup("Thread marker menu", ImGuiWindowFlags_AlwaysAutoResize)) {
        // Global selection
        bool forceSelectAll   = ImGui::Selectable("Select all",   false, ImGuiSelectableFlags_DontClosePopups);
        bool forceDeselectAll = ImGui::Selectable("Deselect all", false, ImGuiSelectableFlags_DontClosePopups);
        ImGui::Separator();

        // Individual selection
        mkr.isFilteredOnThread = false;
        // Loop on thread layout instead of direct thread list, as the layout have sorted threads
        for(int layoutIdx=0; layoutIdx<getConfig().getLayout().size(); ++layoutIdx) {
            const vwConfig::ThreadLayout& ti = getConfig().getLayout()[layoutIdx];
            if(ti.threadId>=cmConst::MAX_THREAD_QTY) continue;
            if(ImGui::Checkbox(getFullThreadName(ti.threadId), &mkr.threadSelection[ti.threadId])) mkr.isCacheDirty = true;
            if(forceSelectAll   && !mkr.threadSelection[ti.threadId]) { mkr.threadSelection[ti.threadId] = true;  mkr.isCacheDirty = true; }
            if(forceDeselectAll &&  mkr.threadSelection[ti.threadId]) { mkr.threadSelection[ti.threadId] = false; mkr.isCacheDirty = true; }
            if(!mkr.threadSelection[ti.threadId]) mkr.isFilteredOnThread = true;
        }
        ImGui::EndPopup();
    }
    // Category filtering
    offsetMenuX += charWidth*(mkr.maxThreadNameLength+1);
    widthMenu = ImGui::CalcTextSize("Category").x;
    DRAWLIST->AddRectFilled(ImVec2(ImGui::GetWindowPos().x+offsetMenuX-padMenuX, textBgY),
                            ImVec2(ImGui::GetWindowPos().x+offsetMenuX+widthMenu+padMenuX, textBgY+ImGui::GetTextLineHeightWithSpacing()), filterBg);
    ImGui::SameLine(offsetMenuX);
    if(mkr.isFilteredOnCategory) ImGui::PushStyleColor(ImGuiCol_Text, vwConst::gold);
    if(ImGui::Selectable("Category", false, 0, ImVec2(widthMenu, 0))) ImGui::OpenPopup("Category marker menu");
    if(mkr.isFilteredOnCategory) ImGui::PopStyleColor();
    if(ImGui::BeginPopup("Category marker menu", ImGuiWindowFlags_AlwaysAutoResize)) {
        // Global selection
        bool forceSelectAll   = ImGui::Selectable("Select all",   false, ImGuiSelectableFlags_DontClosePopups);
        bool forceDeselectAll = ImGui::Selectable("Deselect all", false, ImGuiSelectableFlags_DontClosePopups);
        ImGui::Separator();

        // Individual selection
        mkr.isFilteredOnCategory = false;
        for(int i=0; i<_record->markerCategories.size(); ++i) {
            if(ImGui::Checkbox(_record->getString(_record->markerCategories[i]).value.toChar(), &mkr.categorySelection[i])) mkr.isCacheDirty = true;
            if(forceSelectAll   && !mkr.categorySelection[i]) { mkr.categorySelection[i] = true;  mkr.isCacheDirty = true; }
            if(forceDeselectAll &&  mkr.categorySelection[i]) { mkr.categorySelection[i] = false; mkr.isCacheDirty = true; }
            if(!mkr.categorySelection[i]) mkr.isFilteredOnCategory = true;
        }
        ImGui::EndPopup();
    }

    // Sync combo
    ImGui::SameLine(comboX);
    drawSynchroGroupCombo(comboWidth, &mkr.syncMode);
    ImGui::Separator();

    // Some init
    ImGui::BeginChild("marker", ImVec2(0,0), false,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysVerticalScrollbar);  // Display area is virtual so self-managed
    prepareMarker(mkr); // Ensure cache is up to date, even after window creation
    const double winX = ImGui::GetWindowPos().x;
    const double winY = ImGui::GetWindowPos().y;
    const double winWidth      = ImGui::GetWindowContentRegionMax().x;
    const double winHeight     = ImGui::GetWindowSize().y;
    const double mouseY        = ImGui::GetMousePos().y;
    const bool   isWindowHovered = ImGui::IsWindowHovered();

    constexpr int maxMsgSize = 256;
    char tmpStr [maxMsgSize];

    // Did the user click on the scrollbar? (detection based on an unexpected position change)
    float curScrollPos = ImGui::GetScrollY();
    if(!mkr.didUserChangedScrollPos && !mkr.didUserChangedScrollPosExt && bsAbs(curScrollPos-mkr.lastScrollPos)>=1.) {
        plgScope(MARKER, "New user scroll position from ImGui");
        plgData(MARKER, "expected pos", mkr.lastScrollPos);
        plgData(MARKER, "new pos", curScrollPos);
        mkr.startIdx = curScrollPos/fontHeight;
        mkr.isCacheDirty = true;
    }

    // Manage keys and mouse inputs
    // ============================
    mkr.didUserChangedScrollPos    = mkr.didUserChangedScrollPosExt;
    mkr.didUserChangedScrollPosExt = false;
    int tlWheelCounter = 0;
    if(isWindowHovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // Check mouse input
        // No Ctrl key: wheel is for the text
        int textWheelCounter =  (ImGui::GetIO().KeyCtrl)? 0 : (ImGui::GetIO().MouseWheel*getConfig().getVWheelInversion());
        // Ctrl key: wheel is for the timeline (processed in highlighted text display)
        tlWheelCounter       = (!ImGui::GetIO().KeyCtrl)? 0 : (ImGui::GetIO().MouseWheel*getConfig().getHWheelInversion());
        cmRecord::Evt nextEvt;
        bool eventStatus = false;
        int  dragLineQty = 0;
        if(ImGui::IsMouseDragging(2)) {
            mkr.isDragging = true;
            if(bsAbs(ImGui::GetMouseDragDelta(2).y)>1.) {
                double tmp = (ImGui::GetMouseDragDelta(2).y+mkr.dragReminder);
                ImGui::ResetMouseDragDelta(2);
                dragLineQty      = (int)(tmp/fontHeight);
                mkr.dragReminder = tmp-fontHeight*dragLineQty;
            }
        } else mkr.dragReminder = 0.;

        // Move start position depending on keys, wheel or drag
        if(ImGui::IsKeyPressed(KC_Down) && mkr.startIdx<mkr.maxIdx-1) {
            plgText(MARKER, "Key", "Down pressed");
            cmRecordIteratorMarker it(_record, 0);
            int newStartIdx = mkr.startIdx+1;
            while(newStartIdx<mkr.maxIdx && (eventStatus=it.getEvent(newStartIdx, nextEvt)) &&
                  mkr.isFiltered(nextEvt.threadId, _record->getString(nextEvt.nameIdx).categoryId)) ++newStartIdx;
            if(newStartIdx<mkr.maxIdx && eventStatus) {
                mkr.startIdx                = newStartIdx;
                mkr.isCacheDirty            = true;
                mkr.didUserChangedScrollPos = true;
            }
        }

        if(ImGui::IsKeyPressed(KC_Up) && mkr.startIdx>0) {
            plgText(MARKER, "Key", "Up pressed");
            cmRecordIteratorMarker it(_record, 0);
            int newStartIdx = mkr.startIdx-1;
            while(newStartIdx>=0 && (eventStatus=it.getEvent(newStartIdx, nextEvt)) &&
                  mkr.isFiltered(nextEvt.threadId, _record->getString(nextEvt.nameIdx).categoryId)) --newStartIdx;
            if(newStartIdx>=0 && eventStatus) {
                mkr.startIdx                = newStartIdx;
                mkr.isCacheDirty            = true;
                mkr.didUserChangedScrollPos = true;
            }
        }

        if((textWheelCounter<0 || dragLineQty<0 || ImGui::IsKeyPressed(KC_PageDown)) && mkr.startIdx<mkr.maxIdx-1) {
            plgText(MARKER, "Key", "Page Down pressed");
            const int steps = (dragLineQty!=0)?-dragLineQty:10;
            cmRecordIteratorMarker it(_record, 0);
            int newStartIdx = mkr.startIdx+1;
            for(int i=0; i<steps && mkr.startIdx<mkr.maxIdx; ++i) {
                while(newStartIdx<mkr.maxIdx && (eventStatus=it.getEvent(newStartIdx, nextEvt)) &&
                      mkr.isFiltered(nextEvt.threadId, _record->getString(nextEvt.nameIdx).categoryId)) ++newStartIdx;
                if(newStartIdx<mkr.maxIdx && eventStatus) {
                    mkr.startIdx                = newStartIdx++;
                    mkr.isCacheDirty            = true;
                    mkr.didUserChangedScrollPos = true;
                }
                else break;
            }
        }

        if((textWheelCounter>0 || dragLineQty>0 || ImGui::IsKeyPressed(KC_PageUp)) && mkr.startIdx>0) {
            plgText(MARKER, "Key", "Page Up pressed");
            const int steps = (dragLineQty!=0)?dragLineQty:10;
            cmRecordIteratorMarker it(_record, 0);
            int newStartIdx = mkr.startIdx-1;
            for(int i=0; i<steps && mkr.startIdx>0; ++i) {
                while(newStartIdx>0 && (eventStatus=it.getEvent(newStartIdx, nextEvt)) && mkr.isFiltered(nextEvt.threadId, _record->getString(nextEvt.nameIdx).categoryId)) --newStartIdx;
                if(newStartIdx>=0 && eventStatus) {
                    mkr.startIdx                = newStartIdx--;
                    mkr.isCacheDirty            = true;
                    mkr.didUserChangedScrollPos = true;
                }
            }
        }

        if(!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(KC_F)) {
            plgText(MARKER, "Key", "Full screen pressed");
            setFullScreenView(mkr.uniqueId);
        }

        if(!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(KC_H)) {
            plgText(MARKER, "Key", "Help pressed");
            openHelpTooltip(mkr.uniqueId, "Help Marker");
        }
    }
    else mkr.dragReminder = 0.;

    // Prepare the drawing
    // ===================
    // Previous navigation may have made dirty the cached data
    prepareMarker(mkr);

    // Set the modified scroll position in ImGui, if not changed through imGui
    if(mkr.didUserChangedScrollPos) {
        plgData(MARKER, "Set new scroll pos from user", mkr.startIdx);
        ImGui::SetScrollY(mkr.startIdx*fontHeight);
    }
    // Mark the virtual total size
    mkr.lastScrollPos = ImGui::GetScrollY();
    ImGui::SetCursorPosY(mkr.maxIdx*fontHeight); // Float overrun may create not precise positioning after 2^23 markers (8M), not a big deal
    plgData(MARKER, "Current scroll pos", mkr.lastScrollPos);

    // Compute initial state for all levels
    const bsVec<ImVec4>& palette = getConfig().getColorPalette(true);

    // Draw the marker
    // =============
    float y = winY;
    double mouseTimeBestY      = -1.;
    double mouseTimeBestTimeNs = -1.;
    double newMouseTimeNs      = -1.;
    for(const auto& ci : mkr.cachedItems) {
        const cmRecord::Evt&    evt = ci.evt;
        const cmRecord::String& s   = _record->getString(evt.filenameIdx);
        float heightPix = fontHeight*s.lineQty;

        // Manage hovering: highlight and clicks
        bool doHighlight = isScopeHighlighted(evt.threadId, evt.vS64, evt.flags, -1, evt.nameIdx);

        if(isWindowHovered && mouseY>=y && mouseY<y+heightPix) {
            // Synchronized navigation
            if(mkr.syncMode>0) { // No synchronized navigation for isolated windows
                double syncStartTimeNs, syncTimeRangeNs;
                getSynchronizedRange(mkr.syncMode, syncStartTimeNs, syncTimeRangeNs);

                // Click: set timeline position at middle screen only if outside the center third of screen
                if((ImGui::IsMouseReleased(0) && ImGui::GetMousePos().x<winX+winWidth) || tlWheelCounter) {
                    synchronizeNewRange(mkr.syncMode, bsMax(0., evt.vS64-0.5*syncTimeRangeNs), syncTimeRangeNs);
                    ensureThreadVisibility(mkr.syncMode, evt.threadId);
                }

                // Zoom the timeline
                if(tlWheelCounter!=0) {
                    double newTimeRangeNs = getUpdatedRange(tlWheelCounter, syncTimeRangeNs);
                    synchronizeNewRange(mkr.syncMode, syncStartTimeNs+(evt.vS64-syncStartTimeNs)/syncTimeRangeNs*(syncTimeRangeNs-newTimeRangeNs),
                                        newTimeRangeNs);
                    ensureThreadVisibility(mkr.syncMode, evt.threadId);
                }
            }

            // Right click: contextual menu
            if(!mkr.isDragging && ImGui::IsMouseReleased(2) && ci.elemIdx>=0) {
                mkr.ctxThreadId = evt.threadId;
                mkr.ctxNameIdx  = evt.nameIdx;
                _plotMenuItems.clear(); // Reset the popup menu state
                prepareGraphContextualMenu(ci.elemIdx, 0LL, _record->durationNs, false, false);
                ImGui::OpenPopup("marker menu");
            }

            setScopeHighlight(evt.threadId, evt.vS64, evt.flags, -1, evt.nameIdx);
            doHighlight = true;
        }

        if(doHighlight) {
            // Display some text background if highlighted
            DRAWLIST->AddRectFilled(ImVec2(winX, y), ImVec2(winX+winWidth, y+heightPix), vwConst::uGrey48);
        }

        // Display the date
        double offsetX = winX+textPixMargin;
        snprintf(tmpStr, maxMsgSize, "%f s", 0.000000001*evt.vS64);
        DRAWLIST->AddText(ImVec2(offsetX, y), vwConst::uWhite, tmpStr);
        offsetX += charWidth*14;

        // Display the thread
        snprintf(tmpStr, maxMsgSize, "[%s]", getFullThreadName(evt.threadId));
        DRAWLIST->AddText(ImVec2(offsetX, y), ImColor(getConfig().getThreadColor(evt.threadId, true)), tmpStr);
        offsetX += charWidth*(mkr.maxThreadNameLength+1);

        // Display the category
        DRAWLIST->AddText(ImVec2(offsetX, y), (ci.elemIdx>=0)? (ImU32)ImColor(getConfig().getCurveColor(ci.elemIdx, true)) : vwConst::uGrey,
                          _record->getString(evt.nameIdx).value.toChar());
        offsetX += charWidth*(mkr.maxCategoryLength+1);

        // Display the value
        DRAWLIST->AddText(ImVec2(offsetX, y), ImColor(palette[evt.filenameIdx%palette.size()]), s.value.toChar());

        if(isWindowHovered && mouseY>y) newMouseTimeNs = evt.vS64;
        if(_mouseTimeNs>=evt.vS64 && evt.vS64>mouseTimeBestTimeNs) {
            mouseTimeBestTimeNs = evt.vS64;
            mouseTimeBestY      = y+heightPix;
        }

        // Next line
        if(y>winY+winHeight) break;
        y += heightPix;
    }

    // Display and update the mouse time
    if(mouseTimeBestY>=0) {
        DRAWLIST->AddLine(ImVec2(winX, mouseTimeBestY), ImVec2(winX+winWidth, mouseTimeBestY), vwConst::uYellow);
    }
    if(newMouseTimeNs>=0.) _mouseTimeNs = newMouseTimeNs;
    if(!ImGui::IsMouseDragging(2)) mkr.isDragging = false;


    // Contextual menu
    if(ImGui::BeginPopup("marker menu", ImGuiWindowFlags_AlwaysAutoResize)) {
        double headerWidth = ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("Histogram").x+5;
        ImGui::TextColored(vwConst::grey, "Marker [%s]", _record->getString(mkr.ctxNameIdx).value.toChar());
        ImGui::Separator();

        // Plot & histogram menu
        if(!displayPlotContextualMenu(mkr.ctxThreadId, "Plot", headerWidth)) ImGui::CloseCurrentPopup();
        ImGui::Separator();
        if(!displayHistoContextualMenu(headerWidth)) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // Help
    displayHelpTooltip(mkr.uniqueId, "Help Marker",
                       "##Marker view\n"
                       "===\n"
                       "Displays the global list of markers with filters on categories and threads.\n"
                       "\n"
                       "##Actions:\n"
                       "-#H key#| This help\n"
                       "-#F key#| Full screen view\n"
                       "-#Right mouse button dragging#| Scroll text\n"
                       "-#Up/Down key#| Scroll text\n"
                       "-#Mouse wheel#| Scroll text faster\n"
                       "-#Ctrl-Mouse wheel#| Time zoom views of the same group\n"
                       "-#Left mouse click#| Time synchronize views of the same group\n"
                       "-#Right mouse click#| Open menu for plot/histogram\n"
                       "\n"
                       );

    ImGui::EndChild();
}
