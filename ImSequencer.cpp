#include "ImSequencer.h"
#include <imgui_helper.h>
#include <cmath>

namespace ImSequencer
{
struct SequencerCustomDraw
{
    int index;
    ImRect customRect;
    ImRect legendRect;
    ImRect clippingRect;
    ImRect legendClippingRect;
};

static bool SequencerAddDelButton(ImDrawList *draw_list, ImVec2 pos, bool add = true)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    int delColor = 0xFFFFFFFF; //overDel ? 0xFFAAAAAA : 0x77A3B2AA;
    float midy = pos.y + 16 / 2 - 0.5f;
    float midx = pos.x + 16 / 2 - 0.5f;
    draw_list->AddRect(delRect.Min, delRect.Max, delColor, 4);
    draw_list->AddLine(ImVec2(delRect.Min.x + 3, midy), ImVec2(delRect.Max.x - 4, midy), delColor, 2);
    if (add) draw_list->AddLine(ImVec2(midx, delRect.Min.y + 3), ImVec2(midx, delRect.Max.y - 4), delColor, 2);
    return overDel;
}

bool Sequencer(SequenceInterface *sequence, int *currentFrame, bool *expanded, int *selectedEntry, int *firstFrame, int *lastFrame, int sequenceOptions)
{
    bool ret = false;
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    static float framePixelWidth = 10.f;
    static float framePixelWidthTarget = 10.f;
    int legendWidth = 200;
    static int movingEntry = -1;
    static int movingPos = -1;
    static int movingPart = -1;
    int delEntry = -1;
    int dupEntry = -1;
    int ItemHeight = 20;
    int HeadHeight = 20;
    bool popupOpened = false;
    int sequenceCount = sequence->GetItemCount();
    //if (!sequenceCount) return false;
    ImGui::BeginGroup();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();     // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
    int firstFrameUsed = firstFrame ? *firstFrame : 0;
    int controlHeight = sequenceCount * ItemHeight;
    for (int i = 0; i < sequenceCount; i++)
        controlHeight += int(sequence->GetCustomHeight(i));
    int frameCount = ImMax(sequence->GetFrameMax() - sequence->GetFrameMin(), 1);
    static bool MovingScrollBar = false;
    static bool MovingCurrentFrame = false;
    ImVector<SequencerCustomDraw> customDraws;
    ImVector<SequencerCustomDraw> compactCustomDraws;
    // zoom in/out
    const int visibleFrameCount = (int)floorf((canvas_size.x - legendWidth) / framePixelWidth);
    const float barWidthRatio = ImMin(visibleFrameCount / (float)frameCount, 1.f);
    const float barWidthInPixels = barWidthRatio * (canvas_size.x - legendWidth);
    ImRect regionRect(canvas_pos, canvas_pos + canvas_size);
    static bool panningView = false;
    static ImVec2 panningViewSource;
    static int panningViewFrame;
    ImRect scrollBarRect;
    if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
    {
        if (!panningView)
        {
            panningViewSource = io.MousePos;
            panningView = true;
            panningViewFrame = *firstFrame;
        }
        *firstFrame = panningViewFrame - int((io.MousePos.x - panningViewSource.x) / framePixelWidth);
        *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), sequence->GetFrameMax() - visibleFrameCount);
    }
    if (panningView && !io.MouseDown[2])
    {
        panningView = false;
    }
    framePixelWidthTarget = ImClamp(framePixelWidthTarget, 0.1f, 50.f);
    framePixelWidth = ImLerp(framePixelWidth, framePixelWidthTarget, 0.33f);
    frameCount = sequence->GetFrameMax() - sequence->GetFrameMin();
    if (visibleFrameCount >= frameCount && firstFrame)
        *firstFrame = sequence->GetFrameMin();
    if (lastFrame)
        *lastFrame = firstFrameUsed + visibleFrameCount;
    // --
    if ((expanded && !*expanded) || !sequenceCount)
    {
        ImGui::InvisibleButton("canvas", ImVec2(canvas_size.x - canvas_pos.x, (float)ItemHeight));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
            {
                // check size?
                ImSequencer::SequenceItem * item = (ImSequencer::SequenceItem*)payload->Data;
                ImSequencer::MediaSequence * seq = (ImSequencer::MediaSequence *)sequence;
                SequenceItem * new_item = new SequenceItem(item->mName, item->mPath, item->mFrameStart, item->mFrameEnd, true, item->mMediaType);
                if (new_item->mFrameEnd > sequence->GetFrameMax())
                    sequence->SetFrameMax(new_item->mFrameEnd + 100);
                seq->m_Items.push_back(new_item);
            }
            ImGui::EndDragDropTarget();
        }
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + ItemHeight), 0xFF3D3837, 0);
        char tmps[512];
        ImFormatString(tmps, IM_ARRAYSIZE(tmps), sequence->GetCollapseFmt(), frameCount, sequenceCount);
        draw_list->AddText(ImVec2(canvas_pos.x + 20, canvas_pos.y + 2), 0xFFFFFFFF, tmps);
    }
    else
    {
        bool hasScrollBar(true);
        // test scroll area
        ImVec2 headerSize(canvas_size.x, (float)HeadHeight);
        ImVec2 scrollBarSize(canvas_size.x, 14.f);
        ImGui::InvisibleButton("topBar", headerSize);
        draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, 0xFF000000, 0);
        if (!sequenceCount) 
        {
            ImGui::EndGroup();
            return false;
        }
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(canvas_size.x, canvas_size.y - 8.f - headerSize.y - (hasScrollBar ? scrollBarSize.y : 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::BeginChildFrame(889, childFrameSize, ImGuiWindowFlags_NoScrollbar);
        sequence->focused = ImGui::IsWindowFocused();
        ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x - 14, float(controlHeight)));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
            {
                // check size?
                ImSequencer::SequenceItem * item = (ImSequencer::SequenceItem*)payload->Data;
                ImSequencer::MediaSequence * seq = (ImSequencer::MediaSequence *)sequence;
                auto length = item->mFrameEnd - item->mFrameStart;
                SequenceItem * new_item = new SequenceItem(item->mName, item->mPath, 0, 100, true, item->mMediaType);
                if (currentFrame && firstFrame && *currentFrame >= *firstFrame && *currentFrame <= sequence->GetFrameMax())
                    new_item->mFrameStart = *currentFrame;
                else
                    new_item->mFrameStart = *firstFrame;
                new_item->mFrameEnd = new_item->mFrameStart + length;
                if (new_item->mFrameEnd > sequence->GetFrameMax())
                    sequence->SetFrameMax(new_item->mFrameEnd + 100);
                seq->m_Items.push_back(new_item);
            }
            ImGui::EndDragDropTarget();
        }
        const ImVec2 contentMin = ImGui::GetItemRectMin();
        const ImVec2 contentMax = ImGui::GetItemRectMax();
        const ImRect contentRect(contentMin, contentMax);
        const ImRect legendRect(contentMin, ImVec2(contentMin.x + legendWidth, contentMax.y));
        const float contentHeight = contentMax.y - contentMin.y;
        // full background
        draw_list->AddRectFilled(canvas_pos, canvas_pos + canvas_size, 0xFF242424, 0);
        draw_list->AddRectFilled(legendRect.Min, legendRect.Max, 0xFF121212, 0);

        // current frame top
        ImRect topRect(ImVec2(canvas_pos.x + legendWidth, canvas_pos.y), ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ItemHeight));
        if (!MovingCurrentFrame && !MovingScrollBar && movingEntry == -1 && sequenceOptions & SEQUENCER_CHANGE_FRAME && currentFrame && *currentFrame >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
        {
            MovingCurrentFrame = true;
        }
        if (MovingCurrentFrame)
        {
            if (frameCount)
            {
                *currentFrame = (int)((io.MousePos.x - topRect.Min.x) / framePixelWidth) + firstFrameUsed;
                if (*currentFrame < sequence->GetFrameMin())
                    *currentFrame = sequence->GetFrameMin();
                if (*currentFrame >= sequence->GetFrameMax())
                    *currentFrame = sequence->GetFrameMax();
            }
            if (!io.MouseDown[0])
                MovingCurrentFrame = false;
        }

        //header
        //header frame number and lines
        int modFrameCount = 10;
        int frameStep = 1;
        while ((modFrameCount * framePixelWidth) < 150)
        {
            modFrameCount *= 2;
            frameStep *= 2;
        };
        int halfModFrameCount = modFrameCount / 2;
        auto drawLine = [&](int i, int regionHeight)
        {
            bool baseIndex = ((i % modFrameCount) == 0) || (i == sequence->GetFrameMax() || i == sequence->GetFrameMin());
            bool halfIndex = (i % halfModFrameCount) == 0;
            int px = (int)canvas_pos.x + int(i * framePixelWidth) + legendWidth - int(firstFrameUsed * framePixelWidth);
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : HeadHeight;
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), 0xFF606060, 1);
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)HeadHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), 0x30606060, 1);
            }
            if (baseIndex && px > (canvas_pos.x + legendWidth))
            {
                char tmps[512];
                ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", i);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), 0xFFBBBBBB, tmps);
            }
        };
        auto drawLineContent = [&](int i, int /*regionHeight*/)
        {
            int px = (int)canvas_pos.x + int(i * framePixelWidth) + legendWidth - int(firstFrameUsed * framePixelWidth);
            int tiretStart = int(contentMin.y);
            int tiretEnd = int(contentMax.y);
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), 0x30606060, 1);
            }
        };
        for (int i = sequence->GetFrameMin(); i <= sequence->GetFrameMax(); i += frameStep)
        {
            drawLine(i, HeadHeight);
        }
        drawLine(sequence->GetFrameMin(), HeadHeight);
        drawLine(sequence->GetFrameMax(), HeadHeight);
        // cursor Arrow
        if (currentFrame && firstFrame && *currentFrame >= *firstFrame && *currentFrame <= sequence->GetFrameMax())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (*currentFrame - firstFrameUsed) * framePixelWidth + framePixelWidth / 2 - arrowWidth * 0.5f - 2;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)), ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            char tmps[512];
            ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", *currentFrame);
            ImVec2 str_size = ImGui::CalcTextSize(tmps, nullptr, true) * 0.8;
            float strOffset = contentMin.x + legendWidth + (*currentFrame - firstFrameUsed) * framePixelWidth + framePixelWidth / 2 - str_size.x * 0.5f - 2;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-2, 0), str_pos + str_size + ImVec2(4, 4), ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 0.0f, 0.75f)), 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)), tmps);
            ImGui::SetWindowFontScale(1.0);
        }

        // clip content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);
        // draw item names in the legend rect on the left
        size_t customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            ImVec2 tpos(contentMin.x + 3, contentMin.y + i * ItemHeight + 2 + customHeight);
            draw_list->AddText(tpos, 0xFFFFFFFF, sequence->GetItemLabel(i));
            if (sequenceOptions & SEQUENCER_DEL)
            {
                bool overDel = SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight + 2 - 10, tpos.y + 2), false);
                if (overDel && io.MouseReleased[0])
                    delEntry = i;
            }
            if (sequenceOptions & SEQUENCER_ADD)
            {
                bool overDup = SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight - ItemHeight + 2 - 10, tpos.y + 2), true);
                if (overDup && io.MouseReleased[0])
                    dupEntry = i;
            }
            customHeight += sequence->GetCustomHeight(i);
        }
        // clipping rect so items bars are not visible in the legend on the left when scrolled
        //
        // slots background
        customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            unsigned int col = (i & 1) ? 0xFF3A3636 : 0xFF413D3D;
            size_t localCustomHeight = sequence->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x, pos.y + ItemHeight - 1 + localCustomHeight);
            if (!popupOpened && cy >= pos.y && cy < pos.y + (ItemHeight + localCustomHeight) && movingEntry == -1 && cx > contentMin.x && cx < contentMin.x + canvas_size.x)
            {
                col += 0x80201008;
                pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
        }
        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        // vertical frame lines in content area
        for (int i = sequence->GetFrameMin(); i <= sequence->GetFrameMax(); i += frameStep)
        {
            drawLineContent(i, int(contentHeight));
        }
        drawLineContent(sequence->GetFrameMin(), int(contentHeight));
        drawLineContent(sequence->GetFrameMax(), int(contentHeight));
        // selection
        bool selected = selectedEntry && (*selectedEntry >= 0);
        if (selected)
        {
            customHeight = 0;
            for (int i = 0; i < *selectedEntry; i++)
                customHeight += sequence->GetCustomHeight(i);
            ;
            draw_list->AddRectFilled(ImVec2(contentMin.x, contentMin.y + ItemHeight * *selectedEntry + customHeight), ImVec2(contentMin.x + canvas_size.x, contentMin.y + ItemHeight * (*selectedEntry + 1) + customHeight), 0x80ff4040, 1.f);
        }
        // slots
        customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            int start, end;
            std::string name;
            unsigned int color;
            sequence->Get(i, start, end, name, color);
            size_t localCustomHeight = sequence->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth - firstFrameUsed * framePixelWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 slotP1(pos.x + start * framePixelWidth, pos.y + 2);
            ImVec2 slotP2(pos.x + end * framePixelWidth + framePixelWidth, pos.y + ItemHeight - 2);
            ImVec2 slotP3(pos.x + end * framePixelWidth + framePixelWidth, pos.y + ItemHeight - 2 + localCustomHeight);
            unsigned int slotColor = color | 0xFF000000;
            unsigned int slotColorHalf = (color & 0xFFFFFF) | 0x40000000;
            if (slotP1.x <= (canvas_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
            {
                draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 2);
                draw_list->AddRectFilled(slotP1, slotP2, slotColor, 2);
            }
            if (ImRect(slotP1, slotP2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
            {
                sequence->DoubleClick(i);
            }
            // Ensure grabbable handles
            const float max_handle_width = slotP2.x - slotP1.x / 3.0f;
            const float min_handle_width = ImMin(10.0f, max_handle_width);
            const float handle_width = ImClamp(framePixelWidth / 2.0f, min_handle_width, max_handle_width);
            ImRect rects[3] = {ImRect(slotP1, ImVec2(slotP1.x + handle_width, slotP2.y)), ImRect(ImVec2(slotP2.x - handle_width, slotP1.y), slotP2), ImRect(slotP1, slotP2)};
            const unsigned int quadColor[] = {0xFFFFFFFF, 0xFFFFFFFF, slotColor + (selected ? 0 : 0x202020)};
            if (movingEntry == -1 && (sequenceOptions & SEQUENCER_EDIT_STARTEND)) // TODOFOCUS && backgroundRect.Contains(io.MousePos))
            {
                for (int j = 2; j >= 0; j--)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 2);
                }
                for (int j = 0; j < 3; j++)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                        continue;
                    if (ImGui::IsMouseClicked(0) && !MovingScrollBar && !MovingCurrentFrame)
                    {
                        movingEntry = i;
                        movingPos = cx;
                        movingPart = j + 1;
                        sequence->BeginEdit(movingEntry);
                        break;
                    }
                }
            }
            // custom draw
            if (localCustomHeight > 0)
            {
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + 1 + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstFrameUsed - sequence->GetFrameMin() - 0.5f) * framePixelWidth, float(ItemHeight)),
                                  rp + ImVec2(legendWidth + (sequence->GetFrameMax() - firstFrameUsed - 0.5f + 2.f) * framePixelWidth, float(localCustomHeight + ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(ItemHeight)), rp + ImVec2(canvas_size.x, float(localCustomHeight + ItemHeight)));
                ImRect legendRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight)));
                ImRect legendClippingRect(canvas_pos + ImVec2(0.f, float(ItemHeight + customHeight)), canvas_pos + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight + customHeight)));
                customDraws.push_back({i, customRect, legendRect, clippingRect, legendClippingRect});
            }
            else
            {
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstFrameUsed - sequence->GetFrameMin() - 0.5f) * framePixelWidth, float(0.f)),
                                  rp + ImVec2(legendWidth + (sequence->GetFrameMax() - firstFrameUsed - 0.5f + 2.f) * framePixelWidth, float(ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(0.f)), rp + ImVec2(canvas_size.x, float(ItemHeight)));
                compactCustomDraws.push_back({i, customRect, ImRect(), clippingRect, ImRect()});
            }
            customHeight += localCustomHeight;
        }
        // moving
        if (movingEntry >= 0)
        {
            ImGui::CaptureMouseFromApp();
            int diffFrame = int((cx - movingPos) / framePixelWidth);
            if (std::abs(diffFrame) > 0)
            {
                int start, end;
                std::string name;
                unsigned int color;
                sequence->Get(movingEntry, start, end, name, color);
                if (selectedEntry)
                    *selectedEntry = movingEntry;
                int l = start;
                int r = end;
                if (movingPart & 1)
                    l += diffFrame;
                if (movingPart & 2)
                    r += diffFrame;
                if (l < 0)
                {
                    if (movingPart & 2)
                        r -= l;
                    l = 0;
                }
                if (movingPart & 1 && l > r)
                    l = r;
                if (movingPart & 2 && r < l)
                    r = l;
                movingPos += int(diffFrame * framePixelWidth);
                if (r > sequence->GetFrameMax())
                    sequence->SetFrameMax(r + 100);
                sequence->Set(movingEntry, l, r, name, color);
            }
            if (!io.MouseDown[0])
            {
                // single select
                if (!diffFrame && movingPart && selectedEntry)
                {
                    *selectedEntry = movingEntry;
                    ret = true;
                }
                movingEntry = -1;
                sequence->EndEdit();
            }
        }
        // cursor line
        if (currentFrame && firstFrame && *currentFrame >= *firstFrame && *currentFrame <= sequence->GetFrameMax())
        {
            static const float cursorWidth = 3.f;
            float cursorOffset = contentMin.x + legendWidth + (*currentFrame - firstFrameUsed) * framePixelWidth + framePixelWidth / 2 - cursorWidth * 0.5f - 1;
            draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 128), cursorWidth);
        }
        draw_list->PopClipRect();
        draw_list->PopClipRect();
        for (auto &customDraw : customDraws)
            sequence->CustomDraw(customDraw.index, draw_list, customDraw.customRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect);
        for (auto &customDraw : compactCustomDraws)
            sequence->CustomDrawCompact(customDraw.index, draw_list, customDraw.customRect, customDraw.clippingRect);
        // copy paste
        if (sequenceOptions & SEQUENCER_COPYPASTE)
        {
            ImRect rectCopy(ImVec2(contentMin.x + 100, canvas_pos.y + 2), ImVec2(contentMin.x + 100 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectCopy = rectCopy.Contains(io.MousePos);
            unsigned int copyColor = inRectCopy ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rectCopy.Min, copyColor, "Copy");
            ImRect rectPaste(ImVec2(contentMin.x + 140, canvas_pos.y + 2), ImVec2(contentMin.x + 140 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectPaste = rectPaste.Contains(io.MousePos);
            unsigned int pasteColor = inRectPaste ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rectPaste.Min, pasteColor, "Paste");
            if (inRectCopy && io.MouseReleased[0])
            {
                sequence->Copy();
            }
            if (inRectPaste && io.MouseReleased[0])
            {
                sequence->Paste();
            }
        }
        //
        ImGui::EndChildFrame();
        ImGui::PopStyleColor();
        if (hasScrollBar)
        {
            ImGui::InvisibleButton("scrollBar", scrollBarSize);
            ImVec2 scrollBarMin = ImGui::GetItemRectMin();
            ImVec2 scrollBarMax = ImGui::GetItemRectMax();
            // ratio = number of frames visible in control / number to total frames
            float startFrameOffset = ((float)(firstFrameUsed - sequence->GetFrameMin()) / (float)frameCount) * (canvas_size.x - legendWidth);
            ImVec2 scrollBarA(scrollBarMin.x + legendWidth, scrollBarMin.y - 2);
            ImVec2 scrollBarB(scrollBarMin.x + canvas_size.x, scrollBarMax.y - 1);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF222222, 0);
            scrollBarRect = ImRect(scrollBarA, scrollBarB);
            bool inScrollBar = scrollBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF101010, 8);
            ImVec2 scrollBarC(scrollBarMin.x + legendWidth + startFrameOffset, scrollBarMin.y);
            ImVec2 scrollBarD(scrollBarMin.x + legendWidth + barWidthInPixels + startFrameOffset, scrollBarMax.y - 2);
            draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || MovingScrollBar) ? 0xFF606060 : 0xFF505050, 6);
            ImRect barHandleLeft(scrollBarC, ImVec2(scrollBarC.x + 14, scrollBarD.y));
            ImRect barHandleRight(ImVec2(scrollBarD.x - 14, scrollBarC.y), scrollBarD);
            bool onLeft = barHandleLeft.Contains(io.MousePos);
            bool onRight = barHandleRight.Contains(io.MousePos);
            static bool sizingRBar = false;
            static bool sizingLBar = false;
            draw_list->AddRectFilled(barHandleLeft.Min, barHandleLeft.Max, (onLeft || sizingLBar) ? 0xFFAAAAAA : 0xFF666666, 6);
            draw_list->AddRectFilled(barHandleRight.Min, barHandleRight.Max, (onRight || sizingRBar) ? 0xFFAAAAAA : 0xFF666666, 6);
            ImRect scrollBarThumb(scrollBarC, scrollBarD);
            static const float MinBarWidth = 44.f;
            if (sizingRBar)
            {
                if (!io.MouseDown[0])
                {
                    sizingRBar = false;
                }
                else
                {
                    float barNewWidth = ImMax(barWidthInPixels + io.MouseDelta.x, MinBarWidth);
                    float barRatio = barNewWidth / barWidthInPixels;
                    framePixelWidthTarget = framePixelWidth = framePixelWidth / barRatio;
                    int newVisibleFrameCount = int((canvas_size.x - legendWidth) / framePixelWidthTarget);
                    int lastFrame = *firstFrame + newVisibleFrameCount;
                    if (lastFrame > sequence->GetFrameMax())
                    {
                        framePixelWidthTarget = framePixelWidth = (canvas_size.x - legendWidth) / float(sequence->GetFrameMax() - *firstFrame);
                    }
                }
            }
            else if (sizingLBar)
            {
                if (!io.MouseDown[0])
                {
                    sizingLBar = false;
                }
                else
                {
                    if (fabsf(io.MouseDelta.x) > FLT_EPSILON)
                    {
                        float barNewWidth = ImMax(barWidthInPixels - io.MouseDelta.x, MinBarWidth);
                        float barRatio = barNewWidth / barWidthInPixels;
                        float previousFramePixelWidthTarget = framePixelWidthTarget;
                        framePixelWidthTarget = framePixelWidth = framePixelWidth / barRatio;
                        int newVisibleFrameCount = int(visibleFrameCount / barRatio);
                        int newFirstFrame = *firstFrame + newVisibleFrameCount - visibleFrameCount;
                        newFirstFrame = ImClamp(newFirstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
                        if (newFirstFrame == *firstFrame)
                        {
                            framePixelWidth = framePixelWidthTarget = previousFramePixelWidthTarget;
                        }
                        else
                        {
                            *firstFrame = newFirstFrame;
                        }
                    }
                }
            }
            else
            {
                if (MovingScrollBar)
                {
                    if (!io.MouseDown[0])
                    {
                        MovingScrollBar = false;
                    }
                    else
                    {
                        float framesPerPixelInBar = barWidthInPixels / (float)visibleFrameCount;
                        *firstFrame = int((io.MousePos.x - panningViewSource.x) / framesPerPixelInBar) - panningViewFrame;
                        *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
                    }
                }
                else
                {
                    if (scrollBarThumb.Contains(io.MousePos) && ImGui::IsMouseClicked(0) && firstFrame && !MovingCurrentFrame && movingEntry == -1)
                    {
                        MovingScrollBar = true;
                        panningViewSource = io.MousePos;
                        panningViewFrame = -*firstFrame;
                    }
                    if (!sizingRBar && onRight && ImGui::IsMouseClicked(0))
                        sizingRBar = true;
                    if (!sizingLBar && onLeft && ImGui::IsMouseClicked(0))
                        sizingLBar = true;
                }
            }
        }
    }

    ImGui::EndGroup();
    if (regionRect.Contains(io.MousePos))
    {
        bool overCustomDraw = false;
        bool overScrollBar = false;
        for (auto &custom : customDraws)
        {
            if (custom.customRect.Contains(io.MousePos))
            {
                overCustomDraw = true;
            }
        }
        if (scrollBarRect.Contains(io.MousePos))
        {
            overScrollBar = true;
        }
        if (overScrollBar)
        {
            // up-down wheel over scrollbar, scale canvas view
            int frameOverCursor = *firstFrame + (int)(visibleFrameCount * ((io.MousePos.x - (float)legendWidth - canvas_pos.x) / (canvas_size.x - legendWidth)));
            if (io.MouseWheel < -FLT_EPSILON && visibleFrameCount <= sequence->GetFrameMax())
            {
                *firstFrame -= frameOverCursor;
                *firstFrame = int(*firstFrame * 1.1f);
                framePixelWidthTarget *= 0.9f;
                *firstFrame += frameOverCursor;
            }
            if (io.MouseWheel > FLT_EPSILON)
            {
                *firstFrame -= frameOverCursor;
                *firstFrame = int(*firstFrame * 0.9f);
                framePixelWidthTarget *= 1.1f;
                *firstFrame += frameOverCursor;
            }
        }
        else
        {
            // left-right wheel over blank area, moving canvas view
            if (io.MouseWheelH < -FLT_EPSILON)
            {
                *firstFrame -= visibleFrameCount / 4;
                *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
            }
            if (io.MouseWheelH > FLT_EPSILON)
            {
                *firstFrame += visibleFrameCount / 4;
                *firstFrame = ImClamp(*firstFrame, sequence->GetFrameMin(), ImMax(sequence->GetFrameMax() - visibleFrameCount, sequence->GetFrameMin()));
            }
        }
    }
    if (expanded)
    {
        bool overExpanded = SequencerAddDelButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded);
        if (overExpanded && io.MouseReleased[0])
            *expanded = !*expanded;
    }
    if (delEntry != -1)
    {
        sequence->Del(delEntry);
        if (selectedEntry && (*selectedEntry == delEntry || *selectedEntry >= sequence->GetItemCount()))
            *selectedEntry = -1;
    }
    if (dupEntry != -1)
    {
        sequence->Duplicate(dupEntry);
    }
    return ret;
}

/***********************************************************************************************************
 * SequenceItem Struct Member Functions
 ***********************************************************************************************************/

SequenceItem::SequenceItem(const std::string& name, const std::string& path, int start, int end, bool expand, int type)
{
    mName = name;
    mPath = path;
    mFrameStart = start;
    mFrameEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mMedia = CreateMediaSnapshot();
    if (!path.empty() && mMedia)
    {
        mMedia->Open(path);
    }
    if (mMedia && mMedia->IsOpened())
    {
        mFrameEnd = mMedia->GetVidoeFrameCount();
        mMedia->ConfigSnapWindow(1.f, 10);
    }
}

SequenceItem::~SequenceItem()
{
    if (mMedia && mMedia->IsOpened())
    {
        mMedia->Close();
    }
    ReleaseMediaSnapshot(&mMedia);
    mMedia = nullptr;
    if (mMediaSnapshot)
    {
        ImGui::ImDestroyTexture(mMediaSnapshot); 
        mMediaSnapshot = nullptr;
    }
}

void SequenceItem::SequenceItemUpdateSnapShot()
{
    if (mMediaSnapshot)
        return;
    if (mMedia && mMedia->IsOpened())
    {
        auto pos = (float)mMedia->GetVidoeMinPos()/1000.f;
        std::vector<ImGui::ImMat> snapshots;
        if (mMedia->GetSnapshots(snapshots, pos))
        {
            auto snap = snapshots[snapshots.size() / 2];
            if (!snap.empty())
            {
                ImGui::ImGenerateOrUpdateTexture(mMediaSnapshot, snap.w, snap.h, snap.c, (const unsigned char *)snap.data);
            }
        }
    }
}

/***********************************************************************************************************
 * MediaSequence Struct Member Functions
 ***********************************************************************************************************/

MediaSequence::~MediaSequence()
{
    for (auto item : m_Items)
    {
        delete item;
    }
}

void MediaSequence::Get(int index, int& start, int& end, std::string& name, unsigned int& color)
{
    SequenceItem *item = m_Items[index];
    color = 0xFFAA8080; // same color for everyone, return color based on type
    start = item->mFrameStart;
    end = item->mFrameEnd;
    name = item->mName;
}

void MediaSequence::Set(int index, int start, int end, std::string name, unsigned int color)
{
    SequenceItem *item = m_Items[index];
    //color = 0xFFAA8080; // same color for everyone, return color based on type
    item->mFrameStart = start;
    item->mFrameEnd = end;
    item->mName = name;
}

void MediaSequence::Add(std::string& name)
{ 
    /*m_Items.push_back(SequenceItem{name, "", 0, 10, false});*/ 
}
    
void MediaSequence::Del(int index)
{
    auto item = m_Items.erase(m_Items.begin() + index);
    delete *item;
}
    
void MediaSequence::Duplicate(int index)
{
    /*m_Items.push_back(m_Items[index]);*/
}

void MediaSequence::CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect)
{
    draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
    draw_list->PopClipRect();
    ImGui::SetCursorScreenPos(rc.Min);
}

void MediaSequence::CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect)
{
    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    draw_list->PopClipRect();
}

} // namespace ImSequencer