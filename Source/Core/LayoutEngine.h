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

#ifndef RMLUI_CORE_LAYOUTENGINE_H
#define RMLUI_CORE_LAYOUTENGINE_H

#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBox.h"

namespace Rml {

class Box;

struct FormatSettings {
	const Box* override_initial_box = nullptr;
	Vector2f* out_visible_overflow_size = nullptr;
};

/**
    @author Robert Curry
 */

class LayoutEngine {
public:
	/// Formats the contents for a root-level element, usually a document, absolutely positioned, floating, or replaced element. Establishes a new
	/// block formatting context.
	/// @param[in] element The element to lay out.
	/// @param[in] containing_block The size of the containing block.
	/// @param[in] override_initial_box Optional pointer to a box to override the generated box for the element.
	/// @param[out] visible_overflow_size Optionally output the overflow size of the element.
	static void FormatElement(Element* element, Vector2f containing_block, FormatSettings format_settings = {});

	// TODO Format an element in normal flow layout.
	static bool FormatElementFlow(BlockContainer* block_context_box, Element* element);

	static void* AllocateLayoutChunk(size_t size);
	static void DeallocateLayoutChunk(void* chunk, size_t size);

private:
	// TODO Format an element in an independent formatting context.
	static bool FormatElementBlockified(BlockContainer* block_context_box, Element* element, FormatSettings format_settings);

	/// Formats and positions an element as a block element.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The block element.
	static bool FormatElementBlock(BlockContainer* block_context_box, Element* element, FormatSettings format_settings = {});
	/// Formats and positions an element as an inline element.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The inline element.
	static bool FormatElementInline(BlockContainer* block_context_box, Element* element);
	/// Positions an element as a sized inline element, formatting its internal hierarchy as a block element.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The inline-block element.
	static bool FormatElementInlineBlock(BlockContainer* block_context_box, Element* element);
	/// Formats and positions a flexbox.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The flex container element.
	static bool FormatElementFlex(BlockContainer* block_context_box, Element* element);
	/// Formats and positions a table, including all table-rows and table-cells contained within.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The table element.
	static bool FormatElementTable(BlockContainer* block_context_box, Element* element);
	/// Executes any formatting for special elements.
	/// @param[in] block_context_box The open block box to layout the element in.
	/// @param[in] element The element to parse.
	/// @return True if the element was parsed as a special element, false otherwise.
	static bool FormatElementSpecial(BlockContainer* block_context_box, Element* element);
};

namespace NewLayoutEngine {

	enum class OuterDisplay { BlockLevel, InlineLevel };
	enum class InnerDisplay { BlockContainer, Inline, FlexContainer, TableWrapper, Replaced };
	struct LayoutBox {
		OuterDisplay outer_display;
		InnerDisplay inner_display;

		Vector<LayoutBox> children;
	};

	struct InlineFormattingContext;
	struct BlockFormattingContext;
	struct BlockContainer;

	struct Inline {
		// A single element can be split into multiple inlines (fragments?), for example due to line breaks or due to a block-level DOM descendent.
	};
	struct Line {};

	struct BlockLevelBox : LayoutBox {};

	// Direct child of a block container. Not descibed in CSS, but effectively a "block container that only contains inline-level boxes" or like an
	// "inline box" or perhaps "root inline box"?. The container representing an inline formatting context.
	// Alternatively, call it 'InlineFormattingContext'.
	struct InlineContainer : BlockLevelBox {
		Vector<Line> lines;
		Vector<Inline> inlines;

		// If any block-level boxes are encountered, then we:
		//   1. Close the current inline formatting context.
		//   2. Add the block-level box to our nearest block container.
		BlockContainer* parent;
	};

	// A block container only places block-level boxes. Generated by elements with flow or flow-root inner display.
	struct BlockContainer : BlockLevelBox {
		// Any inline-level boxes encountered establish a new InlineContainer (inline formatting context), unless one is already open.
		Vector<UniquePtr<BlockLevelBox>> blocks;

		BlockFormattingContext* bfc;
	};

	struct FlexContainer {};

	struct BlockLevelFlexContainer : BlockLevelBox {
		FlexContainer flex_container;
	};

	struct InlineLevelFlexContainer : Inline {
		FlexContainer flex_container;
	};

	struct TableWrapper {};

	struct BlockLevelTableWrapper : BlockLevelBox {
		TableWrapper table_wrapper;
	};

	struct InlineLevelTableWrapper : Inline {
		TableWrapper table_wrapper;
	};

	struct BlockFormattingContext {
		Vector2f containing_block;

		Vector<LayoutBox> boxes;

		// Used by block contexts only; stores the block box space pointed to by the 'space' member.
		UniquePtr<LayoutBlockBoxSpace> float_space;

		Vector<Element*> absolutely_positioned_elements;
	};

	// We probably don't need a separate data structure for this?
	struct PrincipalBlockLevelBox : BlockLevelBox {
		Vector<Box> children;
		struct GeneratedContent {};
		GeneratedContent generated_content;
	};

	struct BlockLevelElement {
		Element* element;
		PrincipalBlockLevelBox principal_box;
		Vector<BlockLevelBox> additional_boxes;
	};

} // namespace NewLayoutEngine

} // namespace Rml
#endif
