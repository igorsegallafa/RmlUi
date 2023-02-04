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
static void LogUnexpectedTablePart(Element* element, Style::Display display)
{
	RMLUI_ASSERT(element);
	String value = "*unknown";
	StyleSheetSpecification::GetPropertySpecification().GetProperty(PropertyId::Display)->GetValue(value, Property(display));

	Log::Message(Log::LT_WARNING, "Element has a display type '%s', but is not located in a table. Element will not be formatted: %s", value.c_str(),
		element->GetAddress().c_str());
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

void BlockFormattingContext::Format(Vector2f containing_block, FormatSettings format_settings)
{
	Element* element = GetRootElement();
	RMLUI_ASSERT(element && containing_block.x >= 0 && containing_block.y >= 0);

#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScopedC(0xB22222);
	auto name = CreateString(80, "%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	// TODO: Make a lighter data structure for the containing block, or, just a pointer to the given box's containing block box.
	containing_block_box = MakeUnique<BlockContainer>(LayoutBox::OuterType::BlockLevel, nullptr, nullptr, Box(containing_block), 0.0f, FLT_MAX);
	DebugDumpLayoutTree debug_dump_tree(element, containing_block_box.get());

	for (int layout_iteration = 0; layout_iteration < 2; layout_iteration++)
	{
		if (FormatBlockBox(containing_block_box.get(), element, format_settings))
			break;
	}

	// Close it so that any absolutely positioned or floated elements are placed.
	containing_block_box->Close();

	SubmitElementLayout(element);
}

float BlockFormattingContext::GetShrinkToFitWidth() const
{
	return root_block_container ? root_block_container->GetShrinkToFitWidth() : 0.f;
}

bool BlockFormattingContext::FormatBlockBox(BlockContainer* parent_block_container, Element* element, FormatSettings format_settings)
{
	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_block_container);

	Box box;
	if (format_settings.override_initial_box)
		box = *format_settings.override_initial_box;
	else
		LayoutDetails::BuildBox(box, containing_block, element);

	float min_height, max_height;
	LayoutDetails::GetDefiniteMinMaxHeight(min_height, max_height, element->GetComputedValues(), box, containing_block.y);

	BlockContainer* new_block = parent_block_container->AddBlockBox(element, box, min_height, max_height);
	if (!new_block)
		return false;

	if (!root_block_container)
		root_block_container = new_block; // TODO hacky

	// TODO: In principle, it is possible that we need three iterations: Once to enable horizontal scroll bar, then to
	// enable vertical scroll bar, then finally format with both enabled.
	for (int layout_iteration = 0; layout_iteration < 2; layout_iteration++)
	{
		// Format the element's children.
		for (int i = 0; i < element->GetNumChildren(); i++)
		{
			if (!FormatBlockContainerChild(new_block, element->GetChild(i)))
				i = -1;
		}

		const auto result = new_block->Close();
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

	if (format_settings.out_visible_overflow_size)
		*format_settings.out_visible_overflow_size = new_block->GetVisibleOverflowSize();

	return true;
}

bool BlockFormattingContext::FormatBlockContainerChild(BlockContainer* parent_block, Element* element)
{
#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScoped;
	auto name = CreateString(80, ">%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	// Check for special formatting tags.
	if (element->GetTagName() == "br")
	{
		parent_block->AddBreak();
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
	if (computed.position() == Style::Position::Absolute || computed.position() == Style::Position::Fixed)
	{
		// Display the element as a block element.
		parent_block->AddAbsoluteElement(element);
		return true;
	}

	// If the element is floating, we remove it from the flow.
	if (computed.float_() != Style::Float::None)
	{
		FormatRoot(element, LayoutDetails::GetContainingBlock(parent_block));
		parent_block->AddFloatElement(element);
		return true;
	}

	// The element is nothing exceptional, so format it according to its display property.
	switch (display)
	{
	case Style::Display::Block: return FormatBlockBox(parent_block, element);
	case Style::Display::Inline: return FormatInline(parent_block, element);
	case Style::Display::InlineBlock: return FormatInlineBlock(parent_block, element);
	case Style::Display::Flex: return FormatFlex(parent_block, element);
	case Style::Display::Table: return FormatTable(parent_block, element);

	case Style::Display::TableRow:
	case Style::Display::TableRowGroup:
	case Style::Display::TableColumn:
	case Style::Display::TableColumnGroup:
	case Style::Display::TableCell: LogUnexpectedTablePart(element, display); return true;
	case Style::Display::None: /* handled above */ RMLUI_ERROR; break;
	}

	return true;
}

bool BlockFormattingContext::FormatInline(BlockContainer* parent_block, Element* element)
{
	RMLUI_ZoneScopedC(0x3F6F6F);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_block);

	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BoxContext::Inline);
	auto inline_box_handle = parent_block->AddInlineElement(element, box);

	// Format the element's children.
	for (int i = 0; i < element->GetNumChildren(); i++)
	{
		if (!FormatBlockContainerChild(parent_block, element->GetChild(i)))
			return false;
	}

	parent_block->CloseInlineElement(inline_box_handle);

	return true;
}

bool BlockFormattingContext::FormatInlineBlock(BlockContainer* parent_block, Element* element)
{
	RMLUI_ZoneScopedC(0x1F2F2F);

	// Format the element separately as a block element, then position it inside our own layout as an inline element.
	Vector2f containing_block_size = LayoutDetails::GetContainingBlock(parent_block);

	auto formatting_contex = MakeUnique<BlockFormattingContext>(this, parent_block, element);
	formatting_contex->Format(containing_block_size, {});

	auto inline_box_handle = parent_block->AddInlineElement(element, element->GetBox());
	parent_block->CloseInlineElement(inline_box_handle);

	return true;
}

bool BlockFormattingContext::FormatFlex(BlockContainer* parent_block, Element* element)
{
	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_block);
	RMLUI_ASSERT(containing_block.x >= 0.f);

	auto formatting_context = MakeUnique<FlexFormattingContext>(this, parent_block, element);
	formatting_context->Format(containing_block, FormatSettings{});

	// Add the formatted flex container as a sized block-level element.
	LayoutBox* flex_container = parent_block->AddBlockLevelBox(formatting_context->ExtractContainer(), element, element->GetBox());
	if (!flex_container)
		return false;

	SubmitElementLayout(element);

	return true;
}

bool BlockFormattingContext::FormatTable(BlockContainer* parent_block, Element* element)
{
	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_block);

	auto formatting_context = MakeUnique<TableFormattingContext>(this, parent_block, element);
	formatting_context->Format(containing_block, FormatSettings{});

	// Now that the table is formatted and sized, we can add it to our parent as a block-level element.
	LayoutBox* table_wrapper = parent_block->AddBlockLevelBox(formatting_context->ExtractTableWrapper(), element, element->GetBox());
	if (!table_wrapper)
		return false;

	SubmitElementLayout(element);

	return true;
}

void FormatRoot(Element* element, Vector2f containing_block, FormatSettings format_settings)
{
	using namespace Style;
	auto& computed = element->GetComputedValues();

	const Display display = computed.display();

	if (display == Display::Flex)
	{
		auto formatting_context = MakeUnique<FlexFormattingContext>(nullptr, nullptr, element);
		formatting_context->Format(containing_block, format_settings);
		return;
	}

	if (display == Display::Table)
	{
		auto formatting_context = MakeUnique<TableFormattingContext>(nullptr, nullptr, element);
		formatting_context->Format(containing_block, format_settings);
		return;
	}

	const bool establishes_bfc =
		(computed.float_() != Float::None || computed.position() == Position::Absolute || computed.position() == Position::Fixed ||
			computed.display() == Display::InlineBlock || computed.display() == Display::TableCell || computed.overflow_x() != Overflow::Visible ||
			computed.overflow_y() != Overflow::Visible || !element->GetParentNode() || element->GetParentNode()->GetDisplay() == Display::Flex);

	if (establishes_bfc)
	{
		auto formatting_context = MakeUnique<BlockFormattingContext>(nullptr, nullptr, element);
		formatting_context->Format(containing_block, format_settings);
		return;
	}

	// TODO: Handle gracefully?
	RMLUI_ERROR;
}

} // namespace Rml
