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

#ifndef RMLUI_CORE_LAYOUTLINEBOX_H
#define RMLUI_CORE_LAYOUTLINEBOX_H

#include "LayoutInlineLevelBox.h"

namespace Rml {

class LayoutLineBox {
public:
	LayoutLineBox() {}
	~LayoutLineBox();

	// Returns true if the box should be placed again on a new line.
	bool AddBox(InlineLevelBox* box, bool wrap_content, float line_width, LayoutOverflowHandle& inout_overflow_handle);

	// Returns height of line. Note: This can be different from the element's computed line-height property.
	float Close(Element* offset_parent, Vector2f line_position, float element_line_height);

	UniquePtr<LayoutLineBox> SplitLine();

	// Note: Only inline-boxes need to be closed. Other inline-level boxes does not contain child boxes considered in the current inline formatting
	// context, and does not need to be closed.
	void CloseInlineBox(InlineBox* inline_box);

	float GetBoxCursor() const { return box_cursor; }
	bool IsClosed() const { return is_closed; }

	String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	struct PlacedFragment {
		InlineLevelBox* inline_level_box;
		Vector2f position;      // Outer (top,left) position relative to start of the line, disregarding floats.
		Vector2f layout_bounds; // Outer size for replaced and inline blocks, inner size for inline boxes.

		float spacing_left = 0.f;  // Left margin-border-padding for inline boxes.
		float spacing_right = 0.f; // Right margin-border-padding for inline boxes.

		// @performance Replace by a pointer? Don't need it for most fragments.
		String text;

		bool principal_fragment = true;
		bool has_content = false;
		bool split_left = false;
		bool split_right = false;
	};

	using FragmentList = Vector<PlacedFragment>;

	// The horizontal cursor. This is the outer-right position of the last placed fragment.
	float box_cursor = 0.f;
	// The contribution of opened inline boxes to the placement of the next fragment, due to their left edges (margin-border-padding).
	float open_spacing_left = 0.f;

	// List of placed fragments in this line box.
	FragmentList fragments;

	// List of fragments from inline boxes that have been opened but are yet to be closed.
	// @performance Store using parent pointers to avoid allocations.
	Vector<PlacedFragment> open_fragments;

	bool is_closed = false;
};

} // namespace Rml
#endif
