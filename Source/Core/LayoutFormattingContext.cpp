/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "LayoutFormattingContext.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "LayoutDetails.h"
#include "LayoutFlex.h"
#include "LayoutTable.h"

namespace Rml {

// Table elements should be handled within FormatElementTable, log a warning when it seems like we're encountering table parts in the wild.
static void LogUnexpectedFlowElement(Element* element, Style::Display display)
{
	RMLUI_ASSERT(element);
	String value = "*unknown";
	StyleSheetSpecification::GetPropertySpecification().GetProperty(PropertyId::Display)->GetValue(value, Property(display));

	Log::Message(Log::LT_WARNING, "Element has a display type '%s' which cannot be located in normal flow layout. Element will not be formatted: %s",
		value.c_str(), element->GetAddress().c_str());
}

#ifdef RMLUI_DEBUG
static bool g_debug_dumping_layout_tree = false;
struct DebugDumpLayoutTree {
	Element* element;
	BlockContainer* block_box;
	bool is_printing_tree_root = false;

	DebugDumpLayoutTree(Element* element, BlockContainer* block_box) : element(element), block_box(block_box)
	{
		// When an element with this ID is encountered, dump the formatted layout tree (including all sub-layouts).
		static const String debug_trigger_id = "rmlui-debug-layout";
		is_printing_tree_root = element->HasAttribute(debug_trigger_id);
		if (is_printing_tree_root)
			g_debug_dumping_layout_tree = true;
	}
	~DebugDumpLayoutTree()
	{
		if (g_debug_dumping_layout_tree)
		{
			const String header = ":: " + LayoutDetails::GetDebugElementName(element) + " ::\n";
			const String layout_tree = header + block_box->DumpLayoutTree();
			if (SystemInterface* system_interface = GetSystemInterface())
				system_interface->LogMessage(Log::LT_INFO, layout_tree);

			if (is_printing_tree_root)
				g_debug_dumping_layout_tree = false;
		}
	}
};
#else
struct DebugDumpLayoutTree {
	DebugDumpLayoutTree(Element* /*element*/, BlockContainer* /*block_box*/) {}
};
#endif

enum class OuterDisplayType { BlockLevel, InlineLevel, Invalid };

static OuterDisplayType GetOuterDisplayType(Style::Display display)
{
	switch (display)
	{
	case Style::Display::Flex:
	case Style::Display::Table:
	case Style::Display::Block: return OuterDisplayType::BlockLevel;

	case Style::Display::InlineBlock:
	case Style::Display::Inline: return OuterDisplayType::InlineLevel;

	case Style::Display::TableRow:
	case Style::Display::TableRowGroup:
	case Style::Display::TableColumn:
	case Style::Display::TableColumnGroup:
	case Style::Display::TableCell:
	case Style::Display::None: break;
	}

	return OuterDisplayType::Invalid;
}

UniquePtr<FormattingContext> FormattingContext::ConditionallyCreateIndependentFormattingContext(ContainerBox* parent_container, Element* element)
{
	using namespace Style;
	auto& computed = element->GetComputedValues();

	const Display display = computed.display();

	if (display == Display::Flex)
		return MakeUnique<FlexFormattingContext>(parent_container, element);

	if (display == Display::Table)
		return MakeUnique<TableFormattingContext>(parent_container, element);

	const bool establishes_bfc =
		(computed.float_() != Float::None || computed.position() == Position::Absolute || computed.position() == Position::Fixed ||
			computed.display() == Display::InlineBlock || computed.display() == Display::TableCell || computed.overflow_x() != Overflow::Visible ||
			computed.overflow_y() != Overflow::Visible || !element->GetParentNode() || element->GetParentNode()->GetDisplay() == Display::Flex);

	if (establishes_bfc)
		return MakeUnique<BlockFormattingContext>(parent_container, element);

	return nullptr;
}

void BlockFormattingContext::Format(FormatSettings format_settings)
{
	Element* element = GetRootElement();
	RMLUI_ASSERT(element && !root_block_container);

#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScopedC(0xB22222);
	auto name = CreateString(80, "%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(GetParentBoxOfContext(), element->GetPosition()).size;

	Box box;
	if (format_settings.override_initial_box)
		box = *format_settings.override_initial_box;
	else
		LayoutDetails::BuildBox(box, containing_block, element);

	float min_height, max_height;
	LayoutDetails::GetDefiniteMinMaxHeight(min_height, max_height, element->GetComputedValues(), box, containing_block.y);

	root_block_container = MakeUnique<BlockContainer>(GetParentBoxOfContext(), element, box, min_height, max_height);
	root_block_container->ResetScrollbars(box);

	// Format the element. Since the root box has no block box parent, it should not be possible to require another round of formatting.
	RMLUI_VERIFY(FormatBlockBox(nullptr, element));

	SubmitElementLayout(element);

	if (format_settings.out_visible_overflow_size)
		*format_settings.out_visible_overflow_size = root_block_container->GetVisibleOverflowSize();
}

float BlockFormattingContext::GetShrinkToFitWidth() const
{
	return root_block_container ? root_block_container->GetShrinkToFitWidth() : 0.f;
}

bool BlockFormattingContext::FormatBlockBox(BlockContainer* parent_container, Element* element)
{
	BlockContainer* new_container = nullptr;
	if (parent_container)
	{
		const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_container, element->GetPosition()).size;

		Box box;
		LayoutDetails::BuildBox(box, containing_block, element);
		float min_height, max_height;
		LayoutDetails::GetDefiniteMinMaxHeight(min_height, max_height, element->GetComputedValues(), box, containing_block.y);

		new_container = parent_container->AddBlockBox(element, box, min_height, max_height);
	}
	else
	{
		RMLUI_ASSERT(root_block_container);
		new_container = root_block_container.get();
	}

	if (!new_container)
		return false;

	DebugDumpLayoutTree debug_dump_tree(element, new_container);

	// In rare cases, it is possible that we need three iterations: Once to enable the horizontal scrollbar, then to
	// enable the vertical scrollbar, and then finally to format it with both scrollbars enabled.
	for (int layout_iteration = 0; layout_iteration < 3; layout_iteration++)
	{
		// Format the element's children.
		for (int i = 0; i < element->GetNumChildren(); i++)
		{
			if (!FormatBlockContainerChild(new_container, element->GetChild(i)))
				i = -1;
		}

		const auto result = new_container->Close(parent_container);
		if (result == BlockContainer::CloseResult::LayoutSelf)
			// We need to reformat ourself; do a second iteration to format all of our children and close again.
			continue;
		else if (result == BlockContainer::CloseResult::LayoutParent)
			// We caused our parent to add a vertical scrollbar; bail out!
			return false;

		// Otherwise, we are all good to finish up.
		break;
	}

	SubmitElementLayout(element);

	return true;
}

bool BlockFormattingContext::FormatBlockContainerChild(BlockContainer* parent_container, Element* element)
{
#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScoped;
	auto name = CreateString(80, ">%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	// Check for special formatting tags.
	if (element->GetTagName() == "br")
	{
		parent_container->AddBreak();
		SubmitElementLayout(element);
		return true;
	}

	auto& computed = element->GetComputedValues();
	const Style::Display display = computed.display();

	// Fetch the display property, and don't lay this element out if it is set to a display type of none.
	if (display == Style::Display::None)
		return true;

	// Check for an absolute position; if this has been set, then we remove it from the flow and add it to the current
	// block box to be laid out and positioned once the block has been closed and sized.
	const Style::Position position_property = computed.position();
	if (position_property == Style::Position::Absolute || position_property == Style::Position::Fixed)
	{
		const Vector2f static_position = parent_container->GetOpenStaticPosition(display) - parent_container->GetPosition();
		ContainingBlock containing_block = LayoutDetails::GetContainingBlock(parent_container, position_property);
		containing_block.container->AddAbsoluteElement(element, static_position, parent_container->GetElement());
		return true;
	}

	const OuterDisplayType outer_display = GetOuterDisplayType(display);
	if (outer_display == OuterDisplayType::Invalid)
	{
		LogUnexpectedFlowElement(element, display);
		return true;
	}

	if (auto formatting_context = ConditionallyCreateIndependentFormattingContext(parent_container, element))
	{
		formatting_context->Format({});

		UniquePtr<LayoutBox> layout_box = formatting_context->ExtractRootBox();

		// If the element is floating, we remove it from the flow.
		if (computed.float_() != Style::Float::None)
		{
			parent_container->AddFloatElement(element);
		}
		// Otherwise, check if we have a sized block-level box.
		else if (layout_box && outer_display == OuterDisplayType::BlockLevel)
		{
			if (!parent_container->AddBlockLevelBox(std::move(layout_box), element, element->GetBox()))
				return false;
		}
		// Nope, then this must be an inline-level box.
		else
		{
			RMLUI_ASSERT(outer_display == OuterDisplayType::InlineLevel);
			auto inline_box_handle = parent_container->AddInlineElement(element, element->GetBox());
			parent_container->CloseInlineElement(inline_box_handle);
		}

		// TODO: Not always positioned yet.
		SubmitElementLayout(element);

		return true;
	}

	// The element is an in-flow box participating in this same block formatting context.
	switch (display)
	{
	case Style::Display::Block: return FormatBlockBox(parent_container, element);
	case Style::Display::Inline: return FormatInlineBox(parent_container, element);

	case Style::Display::TableRow:
	case Style::Display::TableRowGroup:
	case Style::Display::TableColumn:
	case Style::Display::TableColumnGroup:
	case Style::Display::TableCell:
	case Style::Display::InlineBlock:
	case Style::Display::Flex:
	case Style::Display::Table:
	case Style::Display::None: /* handled above */ RMLUI_ERROR; break;
	}

	return true;
}

bool BlockFormattingContext::FormatInlineBox(BlockContainer* parent_container, Element* element)
{
	RMLUI_ZoneScopedC(0x3F6F6F);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_container, element->GetPosition()).size;

	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BoxContext::Inline);
	auto inline_box_handle = parent_container->AddInlineElement(element, box);

	// Format the element's children.
	for (int i = 0; i < element->GetNumChildren(); i++)
	{
		if (!FormatBlockContainerChild(parent_container, element->GetChild(i)))
			return false;
	}

	parent_container->CloseInlineElement(inline_box_handle);

	return true;
}

} // namespace Rml
