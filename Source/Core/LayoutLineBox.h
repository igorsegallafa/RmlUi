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

#include "LayoutInlineBox.h"

namespace Rml {

class LayoutLineBox {
public:
	LayoutLineBox() {}
	~LayoutLineBox();

	void AddBox(InlineLevelBox* box, bool wrap_content, float line_width);

	// Returns height of line. Note: This can be different from the element's computed line-height property.
	float Close(Element* offset_parent, Vector2f line_position, float element_line_height);

	void CloseInlineBox(InlineBox* inline_box);

	float GetBoxCursor() const { return box_cursor; }

	bool IsClosed() const { return is_closed; }

	String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	using FragmentIndex = unsigned int;
	static const FragmentIndex InvalidIndex = FragmentIndex(-1);

	template <typename Func>
	void ForAllOpenFragments(Func&& func)
	{
		FragmentIndex fragment_index = open_fragment_index;
		while (PlacedFragment* open_fragment = GetFragment(fragment_index))
		{
			func(*open_fragment);
			fragment_index = open_fragment->parent_index;
		}
	}

	struct PlacedFragment {
		InlineLevelBox* inline_box;
		Vector2f position;      // Outer (top,left) position relative to start of the line, disregarding floats.
		Vector2f layout_bounds; // Outer size for replaced and inline blocks, inner size for inline boxes.

		FragmentIndex parent_index; // Specified for open fragments.

		// @performance Replace by a pointer? Don't need it for most fragments.
		LayoutOverflowHandle overflow_handle;
		String contents;
	};

	PlacedFragment* GetFragment(FragmentIndex index)
	{
		if (index != InvalidIndex)
		{
			RMLUI_ASSERT(index < (FragmentIndex)fragments.size());
			return &fragments[index];
		}
		return nullptr;
	}

	using FragmentList = Vector<PlacedFragment>;

	// Represents a stack of open fragments from nested inline boxes, which will have their width sized to fit their descendants.
	FragmentIndex open_fragment_index = InvalidIndex;

	// The horizontal cursor. This is where the next inline box will be placed along the line.
	float box_cursor = 0.f;

	// The list of inline boxes in this line box. These line boxes may be parented to others in this list.
	FragmentList fragments;

	bool is_closed = false;
};

} // namespace Rml
#endif
