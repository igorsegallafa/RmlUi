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

#ifndef RMLUI_CORE_LAYOUTFLEX_H
#define RMLUI_CORE_LAYOUTFLEX_H

#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutFormattingContext.h"

namespace Rml {

class FlexFormattingContext final : public FormattingContext {
public:
	FlexFormattingContext(FormattingContext* parent_context, LayoutBox* parent_box, Element* element) :
		FormattingContext(Type::Flex, parent_context, parent_box, element)
	{}

	FlexContainer* GetContainer() { return flex_container_box.get(); }
	UniquePtr<FlexContainer> ExtractContainer() { return std::move(flex_container_box); }

	void Format(Vector2f containing_block, FormatSettings format_settings) override;

private:
	/// Format the flexbox and its children.
	/// @param[out] flex_resulting_content_size The final content size of the flex container.
	/// @param[out] flex_content_overflow_size Overflow size in case flex items or their contents overflow the container.
	void Format(Vector2f& flex_resulting_content_size, Vector2f& flex_content_overflow_size) const;

	Vector2f flex_available_content_size;
	Vector2f flex_content_containing_block;
	Vector2f flex_content_offset;
	Vector2f flex_min_size;
	Vector2f flex_max_size;

	UniquePtr<FlexContainer> flex_container_box;
};

} // namespace Rml
#endif
