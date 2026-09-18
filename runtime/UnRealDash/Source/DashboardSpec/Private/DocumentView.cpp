#include "Internal.h"
namespace dashboard_spec {
PropertyView PropertyView::Member(std::string_view name) const {
    const auto *v = static_cast<const Value *>(value_);
    return v && Has(*v, name) ? PropertyView(&At(*v, name)) : PropertyView();
}
PropertyView PropertyView::Element(std::size_t index) const {
    const auto *v = static_cast<const Value *>(value_);
    return v && v->IsArray() && index < v->Size() ? PropertyView(&(*v)[static_cast<rapidjson::SizeType>(index)])
                                                  : PropertyView();
}
std::size_t PropertyView::Size() const {
    const auto *v = static_cast<const Value *>(value_);
    return !v ? 0 : v->IsObject() ? v->MemberCount() : v->IsArray() ? v->Size() : 0;
}
std::string_view PropertyView::Key(std::size_t index) const {
    const auto *v = static_cast<const Value *>(value_);
    if (!v || !v->IsObject() || index >= v->MemberCount())
        return {};
    const auto &key = (v->MemberBegin() + index)->name;
    return {key.GetString(), key.GetStringLength()};
}
bool PropertyView::String(std::string_view &out) const {
    const auto *v = static_cast<const Value *>(value_);
    if (!v || !v->IsString())
        return false;
    out = {v->GetString(), v->GetStringLength()};
    return true;
}
bool PropertyView::Number(double &out) const {
    const auto *v = static_cast<const Value *>(value_);
    if (!v || !v->IsNumber())
        return false;
    out = v->GetDouble();
    return true;
}
bool PropertyView::Boolean(bool &out) const {
    const auto *v = static_cast<const Value *>(value_);
    if (!v || !v->IsBool())
        return false;
    out = v->GetBool();
    return true;
}
namespace {
PropertyView Dashboard(const Document &doc) {
    PropertyView root(&doc.Data().json);
    return root.Member("dashboard").Exists() ? root.Member("dashboard") : root;
}
} // namespace
std::size_t Document::ComponentCount() const { return Dashboard(*this).Member("components").Size(); }
bool Document::ComponentAt(std::size_t index, ComponentView &out) const {
    auto components = Dashboard(*this).Member("components");
    if (index >= components.Size())
        return false;
    ComponentView result;
    result.id = components.Key(index);
    result.node = components.Member(result.id);
    if (!result.node.Member("type").String(result.type))
        return false;
    result.node.Member("parent").String(result.parent);
    result.properties = result.node.Member("properties");
    result.pointer =
        ErrorText(Join(Has(Data().json, "dashboard") ? "/dashboard/components" : "/components", result.id));
    out = std::move(result);
    return true;
}
PropertyView Document::Theme() const { return Dashboard(*this).Member("theme"); }
PropertyView Document::Root() const { return Dashboard(*this); }
} // namespace dashboard_spec
