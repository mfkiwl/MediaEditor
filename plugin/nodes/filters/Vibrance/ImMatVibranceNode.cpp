#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Vibrance_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct VibranceNode final : Node
{
    BP_NODE_WITH_NAME(VibranceNode, "Vibrance", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Color")
    VibranceNode(BP* blueprint): Node(blueprint) { m_Name = "Vibrance"; }

    ~VibranceNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (m_VibranceIn.IsLinked())
        {
            m_vibrance = context.GetPinValue<float>(m_VibranceIn);
        }
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::Vibrance_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_vibrance);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_VibranceIn.m_ID)
        {
            m_VibranceIn.SetValue(m_vibrance);
        }
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        float setting_offset = 320;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        float val = m_vibrance;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_VibranceIn.IsLinked());
        ImGui::SaturationSelector("##slider_vibrance##Vibrance", ImVec2(200, 40), &val, 0.0f, -4.f, 4.f, zoom, 32, 1.0f, true);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_vibrance##Vibrance")) { val = 0.0; changed = true; }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithID("##add_curve_vibrance##Vibrance", key, m_VibranceIn.IsLinked(), "vibrance##Vibrance@" + std::to_string(m_ID), -4.f, 4.f, 0.f, m_VibranceIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (val != m_vibrance) { m_vibrance = val; changed = true; }
        return m_Enabled ? changed : false;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("vibrance"))
        {
            auto& val = value["vibrance"];
            if (val.is_number()) 
                m_vibrance = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["vibrance"] = imgui_json::number(m_vibrance);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue91c"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    FloatPin  m_VibranceIn = { this, "Vibrance"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_VibranceIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    ImGui::Vibrance_vulkan * m_filter   {nullptr};
    float m_vibrance        {0.0f};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(VibranceNode, "Vibrance", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Color")
