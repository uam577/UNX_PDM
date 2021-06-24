// System
#include <cstdio>
#include <algorithm>

// Internal
#include "bsOs.h"
#include "vwFileDialog.h"
#include "vwConst.h"


#ifndef PL_GROUP_FDIAG
#define PL_GROUP_FDIAG 0
#endif

// @#TBC P3 Add a forward/backward navigation?


vwFileDialog::vwFileDialog(const bsString& title, Mode mode, const bsVec<bsString>& typeFilters) :
    _title(title + "##vwFileDialog"), _mode(mode), _typeFilters(typeFilters)
{
    _dirEntries.reserve(128);
    plAssert(!_typeFilters.empty());
}


void
vwFileDialog::open(const bsString& initialPath)
{
    if(_isOpen) return;
    _path        = initialPath.empty()? osGetCurrentPath() : initialPath;
    _driveBitMap = osGetDriveBitmap();
    _shallOpen   = true;
    _shallClose  = false;
    _hasAnswer   = false;
}


bool
vwFileDialog::draw(int fontSize)
{
    static const char* months[13] = { "NULL", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    plgScope(FDIAG, "vwFileDialog::draw");
    bool hasWorked = false;
    ImGui::PushID(this);

    // Handle the opening
    if(_shallOpen) {
        plgText(FDIAG, "State", "Open the popup");
        ImGui::OpenPopup(_title.toChar());
        ImGui::SetNextWindowSize(ImVec2(40*ImGui::GetFontSize(),
                                        bsMin(0.8*ImGui::GetWindowHeight(), 30.*ImGui::GetTextLineHeightWithSpacing())));
        _shallOpen = false;
        _hasAnswer = false;
        _isEntriesDirty = true;
        snprintf(_selection, sizeof(_selection), "%s", _path.toChar());
        _path = osGetDirname(_path);
    }
    float dialogWidth = bsMinMax(fontSize*60., 600., 1200.);
    ImGui::SetNextWindowSize(ImVec2(dialogWidth, 500));
    if(!ImGui::BeginPopupModal(_title.toChar(), 0, ImGuiWindowFlags_NoResize)) {
        ImGui::PopID();
        return false;
    }
    float spacingX = ImGui::GetStyle().ItemSpacing.x;

    // Query the information on the path, if "dirty"
    // =============================================
    bool forceFileSorting = false;
    if(_isEntriesDirty) {
        _dirEntries.clear();
        _fileEntries.clear();
        bsVec<bsDirEntry> entries;
        if(_path.empty()) { // Root case
            if(_driveBitMap) {
                // Creation of the drive letters on Windows
                for(int i=0; i<32; ++i) {
                    if(_driveBitMap&(1<<i)) {
                        _dirEntries.push_back({"A:", true});
                        _dirEntries.back().name[0] += i; // Create the right letter value
                    }
                }
            } else {
                osGetDirContent(PL_DIR_SEP, entries);
            }
        } else {
            osGetDirContent(_path, entries);
        }
        // Dispatch folder and files in the right storage
        for(const bsDirEntry& e : entries) {
            if(e.isDir) _dirEntries.push_back({e.name, {}, 0 });
            else {
                bsString fullPath = _path+bsString(PL_DIR_SEP)+e.name;
                _fileEntries.push_back({e.name, osGetCreationDate(fullPath), (s64)osGetSize(fullPath)});
            }
        }

        // Sort folders
        std::sort(_dirEntries.begin(), _dirEntries.end(),
                  [](const Entry& a, const Entry& b)->bool { return strcasecmp(a.name.toChar(), b.name.toChar())<=0; } );
        forceFileSorting = true;
        _isEntriesDirty = false;
    }

    // First line: the current path with reactive components
    // =====================================================
    bool isRootDisplayed = false, isNewPathSet = false;
    int startOffset = 0, offset = 0;
    bsString newPath, folderName;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    while(offset<_path.size()) {

        // Get the substring
        if(!isRootDisplayed && offset==0) {
            // Special case of root: we keep the separator
            folderName = PL_DIR_SEP;
            if(!_path.empty() && _path[0]==PL_DIR_SEP_CHAR) ++offset; // On Linux, consume the first "/"
            isRootDisplayed = true; // Once...
        } else {
            ImGui::SameLine();
            while(offset<_path.size() && _path[offset]!=PL_DIR_SEP_CHAR) ++offset;
            folderName = bsString((char*)&_path[0]+startOffset, (char*)&_path[0]+offset);
        }

        // Display the selectable folder name
        ImGui::PushID(startOffset);
        ImGui::PushStyleColor(ImGuiCol_Text, vwConst::uYellow);
        if(ImGui::Selectable(folderName.toChar(), false, ImGuiSelectableFlags_DontClosePopups|ImGuiSelectableFlags_AllowDoubleClick,
                             ImVec2(ImGui::CalcTextSize(folderName.toChar()).x,0))) {
            newPath = bsString((char*)&_path[0], (char*)&_path[0]+offset);
            snprintf(_selection, sizeof(_selection), "%s", newPath.toChar());
            isNewPathSet = true;
        }
        ImGui::PopStyleColor();
        if(offset>1) {
            ImGui::SameLine(); ImGui::Text(PL_DIR_SEP);
        }
        ImGui::PopID();

        while(offset<_path.size() && _path[offset]==PL_DIR_SEP_CHAR) ++offset; // Skip the separator
        startOffset = offset;
    }
    ImGui::PopStyleVar();

    // List the folders
    // ================
    float contentHeight = ImGui::GetContentRegionAvail().y-2*ImGui::GetFrameHeightWithSpacing()-2.*2.*ImGui::GetStyle().FramePadding.y;
    ImGui::BeginChild("Content", ImVec2(0.4*dialogWidth, contentHeight), true,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);
    for(auto& e : _dirEntries) {
        if(!_doShowHidden && !e.name.empty() && e.name[0]=='.') continue;
        if(ImGui::Selectable(e.name.toChar(), false, ImGuiSelectableFlags_DontClosePopups|ImGuiSelectableFlags_AllowDoubleClick)) {
            if(_path.empty())                    newPath = e.name;                      // Drive letter case for windows
            else if(_path==bsString(PL_DIR_SEP)) newPath = bsString(PL_DIR_SEP)+e.name; // Root case for linux
            else                                 newPath = _path+bsString(PL_DIR_SEP)+e.name;
            isNewPathSet = true;
            snprintf(_selection, sizeof(_selection), "%s", newPath.toChar());
            if(_mode==SELECT_DIR && ImGui::IsMouseDoubleClicked(0)) { _hasAnswer = true; _shallClose = true; }
        }
    }
    ImGui::EndChild();

    // List the files
    // ==============
    ImGui::SameLine();
    ImGui::BeginChild("File content", ImVec2(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().FramePadding.x, contentHeight), true,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);
    int selectFlags = ImGuiSelectableFlags_DontClosePopups | ((_mode==SELECT_DIR)? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_AllowDoubleClick);
    bsString* filterExtension = (_typeFilters[_selectedFilterIdx].back()!='*')? &_typeFilters[_selectedFilterIdx] : 0;

    if(ImGui::BeginTable("##table files", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY |
                         ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Filename");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Date");
        ImGui::TableHeadersRow();

        // Sort files if required
        if(ImGuiTableSortSpecs* sortsSpecs= ImGui::TableGetSortSpecs()) {
            if(sortsSpecs->SpecsDirty || forceFileSorting) {
                if(!_fileEntries.empty() && sortsSpecs->SpecsCount>0) {
                    int direction = (sortsSpecs->Specs->SortDirection==ImGuiSortDirection_Ascending)? 1 : -1;
                    if(sortsSpecs->Specs->ColumnIndex==0) {
                        std::sort(_fileEntries.begin(), _fileEntries.end(),
                                  [direction](const Entry& a, const Entry& b)->bool { return direction*strcasecmp(a.name.toChar(), b.name.toChar())<=0; } );
                    }
                    if(sortsSpecs->Specs->ColumnIndex==1) {
                        std::sort(_fileEntries.begin(), _fileEntries.end(),
                                  [direction](const Entry& a, const Entry& b)->bool { return (direction*(a.size-b.size))<=0; } );
                    }
                    if(sortsSpecs->Specs->ColumnIndex==2) {
                        std::sort(_fileEntries.begin(), _fileEntries.end(),
                                  [direction](const Entry& a, const Entry& b)->bool { return direction*(a.date.isOlderThan(b.date)?1:-1)>=0; } );
                    }
                }
                sortsSpecs->SpecsDirty = false;
            }
        }

        for(auto& e : _fileEntries) {
            if(!_doShowHidden && !e.name.empty() && e.name[0]=='.') continue;
            // Filter on extension
            if(filterExtension && !e.name.endsWith(*filterExtension, 1)) continue; // 1 = Skip the first character '*'

            // Display the entry
            ImGui::TableNextColumn();
            if(ImGui::Selectable(e.name.toChar(), false, selectFlags)) {
                snprintf(_selection, sizeof(_selection), "%s", (_path+bsString(PL_DIR_SEP)+e.name).toChar());
                if(ImGui::IsMouseDoubleClicked(0)) { _hasAnswer = true; _shallClose = true; }
            }
            ImGui::TableNextColumn();
            ImGui::NextColumn();
            if     (e.size<10000      ) ImGui::Text("%ld B", e.size);
            else if(e.size<10000000   ) ImGui::Text("%.2f KB", 0.001*e.size);
            else if(e.size<10000000000) ImGui::Text("%.2f MB", 0.000001*e.size);
            else                        ImGui::Text("%.2f GB", 0.000000001*e.size);
            ImGui::TableNextColumn();
            ImGui::Text("%s %02d at %02d:%02d:%02d", months[(e.date.month>0 && e.date.month<=12)?e.date.month:0],
                        e.date.day, e.date.hour, e.date.minute, e.date.second);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    // Compute the combo width
    float maxTypeFilterWidth = 0.;
    for(const bsString& s : _typeFilters) maxTypeFilterWidth = bsMax(maxTypeFilterWidth, ImGui::CalcTextSize(s.toChar()).x);
    maxTypeFilterWidth += ImGui::CalcTextSize("OO").x; // Margin for the triangle

    // Current selection
    float comboWidth = maxTypeFilterWidth+2.*spacingX;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-comboWidth-spacingX-ImGui::GetStyle().FramePadding.x);
    if(ImGui::InputText("##Input", _selection, IM_ARRAYSIZE(_selection), ImGuiInputTextFlags_EnterReturnsTrue)) {
        _hasAnswer = true; _shallClose = true;
    }
    // Extension selection
    ImGui::SameLine();
    ImGui::SetNextItemWidth(comboWidth);
    if(ImGui::BeginCombo("##Extension", _typeFilters[_selectedFilterIdx].toChar())) {
        for(int i=0; i<_typeFilters.size(); ++i) {
            if(ImGui::Selectable(_typeFilters[i].toChar(), false)) _selectedFilterIdx = i;
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();

    // Buttons
    if(ImGui::Checkbox("Show hidden items", &_doShowHidden)) _isEntriesDirty = true;
    ImGui::SameLine();
    float cancelWidth = ImGui::CalcTextSize("Cancel").x;
    float selectWidth = ImGui::CalcTextSize("Select").x;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth()-cancelWidth-selectWidth-4*spacingX-ImGui::GetStyle().FramePadding.x);
    if(ImGui::Button("Select")) { _hasAnswer  = true; _shallClose = true; }
    ImGui::SameLine();
    if(ImGui::Button("Cancel")) {
        _selection[0] = 0;
        _hasAnswer  = true;
        _shallClose = true;
    }

    // User selected a reduced path
    if(isNewPathSet) {
        _path           = newPath;
        hasWorked       = true;
        _isEntriesDirty = true;
    }

    // Handle the closing
    if(_shallClose) {
        ImGui::CloseCurrentPopup();
        _shallClose = false;
        hasWorked   = true;
    }
    ImGui::EndPopup();
    ImGui::PopID();

    return hasWorked;
}
