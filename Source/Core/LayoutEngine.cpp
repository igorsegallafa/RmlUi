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

#include "LayoutEngine.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBoxSpace.h"
#include "LayoutDetails.h"
#include "LayoutFlex.h"
#include "LayoutInlineContainer.h"
#include "LayoutTable.h"
#include "Pool.h"
#include <algorithm>
#include <cstddef>
#include <float.h>

namespace Rml {

// TODO: Move to separate file?
template <size_t Size>
struct LayoutChunk {
	alignas(std::max_align_t) byte buffer[Size];
};

static constexpr std::size_t ChunkSizeBig = std::max({sizeof(BlockContainer)});
static constexpr std::size_t ChunkSizeMedium = std::max({sizeof(InlineContainer), sizeof(InlineBox)});
static constexpr std::size_t ChunkSizeSmall =
	std::max({sizeof(InlineLevelBox_Text), sizeof(InlineLevelBox_Atomic), sizeof(LayoutLineBox), sizeof(LayoutBlockBoxSpace)});

static Pool<LayoutChunk<ChunkSizeBig>> layout_chunk_pool_big(50, true);
static Pool<LayoutChunk<ChunkSizeMedium>> layout_chunk_pool_medium(50, true);
static Pool<LayoutChunk<ChunkSizeSmall>> layout_chunk_pool_small(50, true);

void* LayoutEngine::AllocateLayoutChunk(size_t size)
{
	static_assert(ChunkSizeBig > ChunkSizeMedium && ChunkSizeMedium > ChunkSizeSmall, "The following assumes a strict ordering of the chunk sizes.");

	// Note: If any change is made here, make sure a corresponding change is applied to the deallocation procedure below.
	if (size <= ChunkSizeSmall)
		return layout_chunk_pool_small.AllocateAndConstruct();
	else if (size <= ChunkSizeMedium)
		return layout_chunk_pool_medium.AllocateAndConstruct();
	else if (size <= ChunkSizeBig)
		return layout_chunk_pool_big.AllocateAndConstruct();

	RMLUI_ERROR;
	return nullptr;
}

void LayoutEngine::DeallocateLayoutChunk(void* chunk, size_t size)
{
	// Note: If any change is made here, make sure a corresponding change is applied to the allocation procedure above.
	if (size <= ChunkSizeSmall)
		layout_chunk_pool_small.DestroyAndDeallocate((LayoutChunk<ChunkSizeSmall>*)chunk);
	else if (size <= ChunkSizeMedium)
		layout_chunk_pool_medium.DestroyAndDeallocate((LayoutChunk<ChunkSizeMedium>*)chunk);
	else if (size <= ChunkSizeBig)
		layout_chunk_pool_big.DestroyAndDeallocate((LayoutChunk<ChunkSizeBig>*)chunk);
	else
	{
		RMLUI_ERROR;
	}
}

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

// Formats the contents for a root-level element (usually a document or floating element).
void LayoutEngine::FormatElement(Element* element, Vector2f containing_block, FormatSettings format_settings)
{
	RMLUI_ASSERT(element && containing_block.x >= 0 && containing_block.y >= 0);
#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScopedC(0xB22222);
	auto name = CreateString(80, "%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	// TODO: Make a lighter data structure for the containing block, or, just a pointer to the given box's containing block box.
	auto containing_block_box = MakeUnique<BlockContainer>(nullptr, nullptr, Box(containing_block), 0.0f, FLT_MAX);
	DebugDumpLayoutTree debug_dump_tree(element, containing_block_box.get());

	for (int layout_iteration = 0; layout_iteration < 2; layout_iteration++)
	{
		if (FormatElementBlockified(containing_block_box.get(), element, format_settings))
			break;
	}

	// Close it so that any absolutely positioned or floated elements are placed.
	containing_block_box->Close();

	element->OnLayout();
}

bool LayoutEngine::FormatElementFlow(BlockContainer* block_context_box, Element* element)
{
#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScoped;
	auto name = CreateString(80, ">%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	// Check if we have to do any special formatting for any elements that don't fit into the standard layout scheme.
	if (FormatElementSpecial(block_context_box, element))
		return true;

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
		block_context_box->AddAbsoluteElement(element);
		return true;
	}

	// If the element is floating, we remove it from the flow.
	if (computed.float_() != Style::Float::None)
	{
		LayoutEngine::FormatElement(element, LayoutDetails::GetContainingBlock(block_context_box));
		return block_context_box->AddFloatElement(element);
	}

	// The element is nothing exceptional, so format it according to its display property.
	switch (display)
	{
	case Style::Display::Block: return FormatElementBlock(block_context_box, element);
	case Style::Display::Inline: return FormatElementInline(block_context_box, element);
	case Style::Display::InlineBlock: return FormatElementInlineBlock(block_context_box, element);
	case Style::Display::Flex: return FormatElementFlex(block_context_box, element);
	case Style::Display::Table: return FormatElementTable(block_context_box, element);

	case Style::Display::TableRow:
	case Style::Display::TableRowGroup:
	case Style::Display::TableColumn:
	case Style::Display::TableColumnGroup:
	case Style::Display::TableCell: LogUnexpectedTablePart(element, display); return true;
	case Style::Display::None: /* handled above */ RMLUI_ERROR; break;
	}

	return true;
}

bool LayoutEngine::FormatElementBlockified(BlockContainer* block_context_box, Element* element, FormatSettings format_settings)
{
#ifdef RMLUI_ENABLE_PROFILING
	RMLUI_ZoneScoped;
	auto name = CreateString(80, ">%s %x", element->GetAddress(false, false).c_str(), element);
	RMLUI_ZoneName(name.c_str(), name.size());
#endif

	const Style::Display display = element->GetDisplay();

	switch (display)
	{
	case Style::Display::Inline:
	case Style::Display::InlineBlock:
	case Style::Display::TableCell:
	case Style::Display::Block: return FormatElementBlock(block_context_box, element, format_settings);
	case Style::Display::Flex: return FormatElementFlex(block_context_box, element);
	case Style::Display::Table: return FormatElementTable(block_context_box, element);

	case Style::Display::TableRow:
	case Style::Display::TableRowGroup:
	case Style::Display::TableColumn:
	case Style::Display::TableColumnGroup: LogUnexpectedTablePart(element, display); return true;
	case Style::Display::None: break;
	}

	return true;
}

bool LayoutEngine::FormatElementBlock(BlockContainer* block_context_box, Element* element, FormatSettings format_settings)
{
	RMLUI_ZoneScopedC(0x2F4F4F);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(block_context_box);

	Box box;
	if (format_settings.override_initial_box)
		box = *format_settings.override_initial_box;
	else
		LayoutDetails::BuildBox(box, containing_block, element);

	float min_height, max_height;
	LayoutDetails::GetDefiniteMinMaxHeight(min_height, max_height, element->GetComputedValues(), box, containing_block.y);

	BlockContainer* new_block_context_box = block_context_box->AddBlockElement(element, box, min_height, max_height);
	if (!new_block_context_box)
		return false;

	for (int layout_iteration = 0; layout_iteration < 2; layout_iteration++)
	{
		// Format the element's children.
		for (int i = 0; i < element->GetNumChildren(); i++)
		{
			if (!FormatElementFlow(new_block_context_box, element->GetChild(i)))
				i = -1;
		}

		const auto result = new_block_context_box->Close();
		if (result == BlockContainer::CloseResult::LayoutSelf)
			// We need to reformat ourself; do a second iteration to format all of our children and close again.
			continue;
		else if (result == BlockContainer::CloseResult::LayoutParent)
			// We caused our parent to add a vertical scrollbar; bail out!
			return false;

		// Otherwise, we are all good to finish up.
		break;
	}

	element->OnLayout();

	if (format_settings.out_visible_overflow_size)
		*format_settings.out_visible_overflow_size = new_block_context_box->GetVisibleOverflowSize();

	return true;
}

bool LayoutEngine::FormatElementInline(BlockContainer* block_context_box, Element* element)
{
	RMLUI_ZoneScopedC(0x3F6F6F);

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(block_context_box);

	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BoxContext::Inline);
	auto inline_box_handle = block_context_box->AddInlineElement(element, box);

	// Format the element's children.
	for (int i = 0; i < element->GetNumChildren(); i++)
	{
		if (!FormatElementFlow(block_context_box, element->GetChild(i)))
			return false;
	}

	block_context_box->CloseInlineElement(inline_box_handle);

	return true;
}

bool LayoutEngine::FormatElementInlineBlock(BlockContainer* block_context_box, Element* element)
{
	RMLUI_ZoneScopedC(0x1F2F2F);

	// Format the element separately as a block element, then position it inside our own layout as an inline element.
	Vector2f containing_block_size = LayoutDetails::GetContainingBlock(block_context_box);

	FormatElement(element, containing_block_size);

	auto inline_box_handle = block_context_box->AddInlineElement(element, element->GetBox());
	block_context_box->CloseInlineElement(inline_box_handle);

	return true;
}

bool LayoutEngine::FormatElementFlex(BlockContainer* block_context_box, Element* element)
{
	const ComputedValues& computed = element->GetComputedValues();
	const Vector2f containing_block = LayoutDetails::GetContainingBlock(block_context_box);
	RMLUI_ASSERT(containing_block.x >= 0.f);

	// Build the initial box as specified by the flex's style, as if it was a normal block element.
	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BoxContext::Block);

	Vector2f min_size, max_size;
	LayoutDetails::GetMinMaxWidth(min_size.x, max_size.x, computed, box, containing_block.x);
	LayoutDetails::GetMinMaxHeight(min_size.y, max_size.y, computed, box, containing_block.y);

	// Add the flex container element as if it was a normal block element.
	BlockContainer* flex_block_context_box = block_context_box->AddBlockElement(element, box, min_size.y, max_size.y);
	if (!flex_block_context_box)
		return false;

	// Format the flexbox and all its children.
	ElementList absolutely_positioned_elements, relatively_positioned_elements;
	Vector2f formatted_content_size, content_overflow_size;
	LayoutFlex::Format(box, min_size, max_size, containing_block, element, formatted_content_size, content_overflow_size,
		absolutely_positioned_elements, relatively_positioned_elements);

	// Set the box content size to match the one determined by the formatting procedure.
	flex_block_context_box->GetBox().SetContent(formatted_content_size);
	// Set the inner content size so that any overflow can be caught.
	flex_block_context_box->ExtendInnerContentSize(content_overflow_size);

	// Finally, add any absolutely and relatively positioned flex items.
	for (Element* abs_element : absolutely_positioned_elements)
		flex_block_context_box->AddAbsoluteElement(abs_element);

	flex_block_context_box->AddRelativeElements(std::move(relatively_positioned_elements));

	// Close the block box, this may result in scrollbars being added to ourself or our parent.
	const auto close_result = flex_block_context_box->Close();
	if (close_result == BlockContainer::CloseResult::LayoutParent)
	{
		// Scollbars added to parent, bail out to reformat all its children.
		return false;
	}
	else if (close_result == BlockContainer::CloseResult::LayoutSelf)
	{
		// Scrollbars added to flex container, it needs to be formatted again to account for changed width or height.
		absolutely_positioned_elements.clear();
		relatively_positioned_elements.clear();

		LayoutFlex::Format(box, min_size, max_size, containing_block, element, formatted_content_size, content_overflow_size,
			absolutely_positioned_elements, relatively_positioned_elements);

		flex_block_context_box->GetBox().SetContent(formatted_content_size);
		flex_block_context_box->ExtendInnerContentSize(content_overflow_size);

		if (flex_block_context_box->Close() == BlockContainer::CloseResult::LayoutParent)
			return false;
	}

	element->OnLayout();

	return true;
}

bool LayoutEngine::FormatElementTable(BlockContainer* block_context_box, Element* element_table)
{
	const ComputedValues& computed_table = element_table->GetComputedValues();

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(block_context_box);

	// Build the initial box as specified by the table's style, as if it was a normal block element.
	Box box;
	LayoutDetails::BuildBox(box, containing_block, element_table, BoxContext::Block);

	Vector2f min_size, max_size;
	LayoutDetails::GetMinMaxWidth(min_size.x, max_size.x, computed_table, box, containing_block.x);
	LayoutDetails::GetMinMaxHeight(min_size.y, max_size.y, computed_table, box, containing_block.y);
	const Vector2f initial_content_size = box.GetSize();

	ElementList relatively_positioned_elements;

	// Format the table, this may adjust the box content size.
	const Vector2f table_content_overflow_size = LayoutTable::FormatTable(box, min_size, max_size, element_table, relatively_positioned_elements);

	const Vector2f final_content_size = box.GetSize();
	RMLUI_ASSERT(final_content_size.y >= 0);

	if (final_content_size != initial_content_size)
	{
		// Perform this step to re-evaluate any auto margins.
		LayoutDetails::BuildBoxSizeAndMargins(box, min_size, max_size, containing_block, element_table, BoxContext::Block, true);
	}

	// Now that the box is finalized, we can add table as a block element. If we did it earlier, eg. just before formatting the table,
	// then the table element's offset would not be correct in cases where table size and auto-margins were adjusted.
	BlockContainer* table_block_context_box = block_context_box->AddBlockElement(element_table, box, final_content_size.y, final_content_size.y);
	if (!table_block_context_box)
		return false;

	// Set the inner content size so that any overflow can be caught.
	table_block_context_box->ExtendInnerContentSize(table_content_overflow_size);

	// Add any relatively positioned elements so that their positions are correctly resolved against the table size, acting as their containing block.
	table_block_context_box->AddRelativeElements(std::move(relatively_positioned_elements));

	// If the close failed, it probably means that its parent produced scrollbars.
	if (table_block_context_box->Close() != BlockContainer::CloseResult::OK)
		return false;

	return true;
}

bool LayoutEngine::FormatElementSpecial(BlockContainer* block_context_box, Element* element)
{
	static const String br("br");

	// Check for a <br> tag.
	if (element->GetTagName() == br)
	{
		block_context_box->AddBreak();
		element->OnLayout();
		return true;
	}

	return false;
}

} // namespace Rml
