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
#include "LayoutFormattingContext.h" // TODO: Remove, only used for FormatSettings

namespace Rml {

class Box;

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
	//static bool FormatElementFlow(BlockContainer* block_context_box, Element* element);

	static void* AllocateLayoutChunk(size_t size);
	static void DeallocateLayoutChunk(void* chunk, size_t size);
};

} // namespace Rml
#endif
