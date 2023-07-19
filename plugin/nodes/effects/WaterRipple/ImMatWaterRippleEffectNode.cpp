#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "WaterRipple_vulkan.h"
#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct WaterRippleEffectNode final : Node
{
    BP_NODE_WITH_NAME(WaterRippleEffectNode, "WaterRipple Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    WaterRippleEffectNode(BP* blueprint): Node(blueprint) { m_Name = "WaterRipple Effect"; }

    ~WaterRippleEffectNode()
    {
        if (m_effect) { delete m_effect; m_effect = nullptr; }
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
        if (m_TimeIn.IsLinked()) m_time = context.GetPinValue<float>(m_TimeIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_effect || gpu != m_device)
            {
                if (m_effect) { delete m_effect; m_effect = nullptr; }
                m_effect = new ImGui::WaterRipple_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, m_time, m_freq, m_amount, m_speed);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_TimeIn.m_ID)
        {
            m_TimeIn.SetValue(m_time);
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

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _time = m_time;
        float _freq = m_freq;
        float _amount = m_amount;
        float _speed = m_speed;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_TimeIn.IsLinked());
        ImGui::SliderFloat("Time##WaterRipple", &_time, 0.0, 10.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_time##WaterRipple")) { _time = 10.f; changed = true; }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithID("##add_curve_time##WaterRipple", key, m_TimeIn.IsLinked(), "time##WaterRipple@" + std::to_string(m_ID), 0.0f, 100.f, 1.f, m_TimeIn.m_ID);
        ImGui::EndDisabled();
        ImGui::SliderFloat("Freq##WaterRipple", &_freq, 0.0, 40.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_freq##WaterRipple")) { _freq = 24.f; changed = true; }
        ImGui::SliderFloat("Amount##WaterRipple", &_amount, 0.0, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amount##WaterRipple")) { _amount = 0.03f; changed = true; }
        ImGui::SliderFloat("Speed##WaterRipple", &_speed, 1.f, 10.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_speed##WaterRipple")) { _speed = 3.f; changed = true; }
        ImGui::PopItemWidth();
        if (_time != m_time) { m_time = _time; changed = true; }
        if (_freq != m_freq) { m_freq = _freq; changed = true; }
        if (_amount != m_amount) { m_amount = _amount; changed = true; }
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
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
        if (value.contains("time"))
        {
            auto& val = value["time"];
            if (val.is_number()) 
                m_time = val.get<imgui_json::number>();
        }
        if (value.contains("freq"))
        {
            auto& val = value["freq"];
            if (val.is_number()) 
                m_freq = val.get<imgui_json::number>();
        }
        if (value.contains("amount"))
        {
            auto& val = value["amount"];
            if (val.is_number()) 
                m_amount = val.get<imgui_json::number>();
        }
        if (value.contains("speed"))
        {
            auto& val = value["speed"];
            if (val.is_number()) 
                m_speed = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["time"] = imgui_json::number(m_time);
        value["freq"] = imgui_json::number(m_freq);
        value["amount"] = imgui_json::number(m_amount);
        value["speed"] = imgui_json::number(m_speed);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf198"));
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
    FloatPin  m_TimeIn  = { this, "Time" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_TimeIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_time            {10.f};
    float m_freq            {24.f};
    float m_amount          {0.03f};
    float m_speed           {3.f};
    ImGui::WaterRipple_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(WaterRippleEffectNode, "WaterRipple Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")