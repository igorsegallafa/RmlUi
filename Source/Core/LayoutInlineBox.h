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

#ifndef RMLUI_CORE_LAYOUTINLINEBOX_H
#define RMLUI_CORE_LAYOUTINLINEBOX_H

#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/StyleTypes.h"

namespace Rml {

class Element;
struct LayoutFragment;

using LayoutOverflowHandle = int;

class InlineLevelBox {
public:
	virtual ~InlineLevelBox();

	virtual LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width) = 0;

	virtual float GetEdge(Box::Edge edge) const;

	virtual String DebugDumpNameValue() const = 0;
	virtual String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	InlineLevelBox(Element* element) : element(element) {}

	Element* GetElement() const { return element; }

private:
	Element* element;
};

// TODO: InlineBox
class LayoutInlineBox : public InlineLevelBox {
public:
	InlineLevelBox* AddChild(UniquePtr<InlineLevelBox> child)
	{
		auto result = child.get();
		children.push_back(std::move(child));
		return result;
	}

	String DebugDumpTree(int depth) const override;

protected:
	LayoutInlineBox(Element* element) : InlineLevelBox(element) {}

private:
	using InlineBoxList = Vector<UniquePtr<InlineLevelBox>>;

	// @performance Use first_child, next_sibling instead to build the tree?
	InlineBoxList children;
};

class InlineBox_Root final : public LayoutInlineBox {
public:
	InlineBox_Root() : LayoutInlineBox(nullptr) {}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width) override;

	String DebugDumpNameValue() const override { return "InlineBox_Root"; }
};

class InlineBox_Element final : public LayoutInlineBox {
public:
	InlineBox_Element(Element* element, const Box& box) : LayoutInlineBox(element)
	{
		RMLUI_ASSERT(element);
		RMLUI_ASSERT(box.GetSize().x < 0.f);
	}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width) override;
	float GetEdge(Box::Edge edge) const override;

	String DebugDumpNameValue() const override { return "InlineBox_Element"; }

private:
	Box box;
};

class InlineLevelBox_Replaced final : public InlineLevelBox {
public:
	InlineLevelBox_Replaced(Element* element, const Box& box) : InlineLevelBox(element), box(box)
	{
		RMLUI_ASSERT(element);
		RMLUI_ASSERT(box.GetSize() >= Vector2f(0.f));
	}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width) override;

	float GetEdge(Box::Edge edge) const override;

	String DebugDumpNameValue() const override { return "InlineLevelBox_Replaced"; }

private:
	Box box;
};

struct LayoutFragment {
	enum class Type {
		Invalid,
		Closed,
	};

	LayoutFragment() = default;
	LayoutFragment(InlineLevelBox* inline_box, Vector2f outer_size, LayoutOverflowHandle overflow_handle = {}) :
		inline_box(inline_box), outer_size(outer_size), overflow_handle(overflow_handle)
	{}

	explicit operator bool() const { return inline_box; }

	InlineLevelBox* inline_box = nullptr;
	Vector2f outer_size;

	// Overflow handle is non-zero when there is another fragment to be layed out.
	// TODO: I think we can make this part of the return value for LayoutContent instead? No need to keep this around. Maybe need a pointer to the
	// next fragment in the chain.
	LayoutOverflowHandle overflow_handle = {};
};

String LayoutElementName(Element* element);

} // namespace Rml
#endif
