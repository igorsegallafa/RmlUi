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

#ifndef RMLUI_CORE_LAYOUTFORMATTINGCONTEXT_H
#define RMLUI_CORE_LAYOUTFORMATTINGCONTEXT_H

#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBox.h"

namespace Rml {

class Box;

struct FormatSettings {
	ContainerBox* parent_container = nullptr; // TODO: A bit hacky to have this here.
	const Box* override_initial_box = nullptr;
	Vector2f* out_visible_overflow_size = nullptr;
};

class FormattingContext {
public:
	enum class Type {
		Block,
		Inline,
		Table,
		Flex,
	};
	enum class SizingMode {
		StretchFit,
		MinContent,
		MaxContent,
	};

	// TODO: Consider working directly with the final types instead of using a virtual destructor.
	virtual ~FormattingContext() = default;

	// TODO: Instead of (output) format settings, use function calls (virtual if necessary) as needed.
	virtual void Format(Vector2f containing_block, FormatSettings format_settings) = 0;

	virtual UniquePtr<LayoutBox> ExtractRootBox() { return nullptr; }

protected:
	FormattingContext(Type type, FormattingContext* parent_context, const LayoutBox* parent_box, Element* root_element) :
		type(type), parent_context(parent_context), parent_box(parent_box), root_element(root_element)
	{}

	Element* GetRootElement() const { return root_element; }

	static void SubmitElementLayout(Element* element) { element->OnLayout(); }

private:
	Type type;
	FormattingContext* parent_context;
	const LayoutBox* parent_box;
	Element* root_element;
};

class BlockFormattingContext final : public FormattingContext {
public:
	BlockFormattingContext(FormattingContext* parent_context, const LayoutBox* parent_box, Element* element) :
		FormattingContext(Type::Block, parent_context, parent_box, element)
	{
		RMLUI_ASSERT(element);
	}

	void Format(Vector2f containing_block, FormatSettings format_settings) override;

	float GetShrinkToFitWidth() const;

	UniquePtr<LayoutBox> ExtractRootBox() override { return std::move(root_block_container); }

private:
	bool FormatBlockBox(BlockContainer* parent_container, Element* element, FormatSettings format_settings = {}, Vector2f root_containing_block = {});

	bool FormatBlockContainerChild(BlockContainer* parent_container, Element* element);
	bool FormatInlineBox(BlockContainer* parent_container, Element* element);

	UniquePtr<BlockContainer> root_block_container;
};

// Formats the contents for a root-level element (usually a document or floating element).
void FormatRoot(Element* element, Vector2f containing_block, FormatSettings format_settings = {});

} // namespace Rml
#endif
