/*
 *   Copyright 2018 Mikhail Vasilyev
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#ifndef XMLPARSER_HPP
#define XMLPARSER_HPP

#include <algorithm>
#include <utility>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <istream>
#include <sstream>
#include <cctype>
#include <cwctype>
#include <list>

namespace xml {
namespace details {

// Some helpers for resolving the correct standard library functions

template <typename TChar>
bool IsSpace(TChar symbol)
{
   unsigned val = static_cast<unsigned>(symbol);
   return val >= 9 && val <= 13;
}

#define IS_SPACE(func, type)              \
   template <>                            \
   inline bool IsSpace<type>(type symbol) \
   {                                      \
      return func(symbol);                \
   }

IS_SPACE(std::isspace, char)
IS_SPACE(std::iswspace, wchar_t)

#undef IS_SPACE


template <typename TChar>
bool IsAlpha(TChar symbol)
{
   unsigned val = static_cast<unsigned>(symbol);
   return (val >= 65 && val <= 90) || (val >= 97 && val <= 122);
}

#define IS_ALPHA(func, type)              \
   template <>                            \
   inline bool IsAlpha<type>(type symbol) \
   {                                      \
      return func(symbol);                \
   }

IS_ALPHA(std::isalpha, char)
IS_ALPHA(std::iswalpha, wchar_t)

#undef IS_ALPHA

// Tables for different encodings, mapping entity references to ascii symbols
template <typename TChar>
const TChar **EntityRefTable(std::size_t index) noexcept;

#define ENTITY_REF_TABLE(prefix, type)                                                                       \
   template <>                                                                                               \
   inline const type **EntityRefTable<type>(std::size_t index) noexcept                                      \
   {                                                                                                         \
      static const type *table[][4] = {{prefix##"&amp;", prefix##"&#38;", prefix##"&#x26;", prefix##"&"},    \
                                       {prefix##"&lt;", prefix##"&#60;", prefix##"&#x3C;", prefix##"<"},     \
                                       {prefix##"&gt;", prefix##"&#62;", prefix##"&#x3E;", prefix##">"},     \
                                       {prefix##"&quot;", prefix##"&#34;", prefix##"&#x22;", prefix##"\""},  \
                                       {prefix##"&apos;", prefix##"&#39;", prefix##"&#x27;", prefix##"\'"}}; \
      return table[index];                                                                                   \
   }

ENTITY_REF_TABLE(, char)
ENTITY_REF_TABLE(L, wchar_t)
ENTITY_REF_TABLE(u, char16_t)
ENTITY_REF_TABLE(U, char32_t)

#undef ENTITY_REF_TABLE

// Keys of attributes in xml declaration, in different encodings
template <typename TChar>
const std::basic_string<TChar> *DeclarationAttrs() noexcept;

#define DECLARATION_ATTRS(prefix, type)                                                           \
   template <>                                                                                    \
   inline const std::basic_string<type> *DeclarationAttrs<type>() noexcept                        \
   {                                                                                              \
      static const std::basic_string<type> decl_attrs[] = {prefix##"version", prefix##"encoding", \
                                                           prefix##"standalone"};                 \
      return decl_attrs;                                                                          \
   }

DECLARATION_ATTRS(, char)
DECLARATION_ATTRS(L, wchar_t)
DECLARATION_ATTRS(u, char16_t)
DECLARATION_ATTRS(U, char32_t)

#undef DECLARATION_ATTRS

// XML markup for writing
template <typename TChar>
const TChar *MarkupTable(std::size_t index) noexcept;

#define MARKUP_TABLE(prefix, type)                                                                           \
   template <>                                                                                               \
   inline const type *MarkupTable<type>(std::size_t index) noexcept                                          \
   {                                                                                                         \
      static const type *table[] = {prefix##"<", prefix##">",  prefix##" />",   prefix##"</", prefix##"=\"", \
                                    prefix##" ", prefix##"\"", prefix##"<?xml", prefix##" ?>"};              \
      return table[index];                                                                                   \
   }

MARKUP_TABLE(, char)
MARKUP_TABLE(L, wchar_t)
MARKUP_TABLE(u, char16_t)
MARKUP_TABLE(U, char32_t)

enum Markup : size_t
{
   OPENING_TAG_START = 0, // <
   SINGLE_TAG_END    = 2, // _/>
   OPENING_TAG_END   = 1, // >
   CLOSING_TAG_START = 3, // </
   CLOSING_TAG_END   = 1, // >
   ATTR_START        = 5, // _
   ATTR_END          = 6, // "
   ATTR_MID          = 4, // ="
   DECL_START        = 7, // <?xml
   DECL_END          = 8  // _?>
};

#undef MARKUP_TABLE



// Creates a list of pointers: each one pointing either to a '<' or behind a '>'.
// Text between each pair of successive pointers is a token.
template <typename TChar>
std::list<const TChar *> Tokenize(const TChar *text)
{
   std::list<const TChar *> tokens;
   tokens.emplace_back(text);
   const TChar *pit;
   for (pit = text + 1; *pit; ++pit) {
      if (*pit == '<')
         tokens.emplace_back(pit);
      else if (*(pit - 1) == '>')
         tokens.emplace_back(pit);
   }
   if (*tokens.back()) {
      // add null-terminator
      tokens.emplace_back(pit);
   }
   return tokens;
}

// Removes pointers surrounding regions of whitespaces, so that no tokens consist entirely of
// whitespaces.
template <typename TChar>
void RemoveGaps(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   while (it_right != tokens->end()) {
      bool wspace_only = std::all_of(*it_left, *it_right, IsSpace<TChar>);
      ++it_right;
      if (wspace_only)
         it_left = tokens->erase(it_left);
      else
         ++it_left;
   }
}

// Detect comment start and end. Using strncmp or std::equal was much slower!
template <typename TChar>
inline bool IsCommentStart(const TChar *pit) noexcept
{
   return pit[0] == (TChar)'<' && pit[1] == (TChar)'!' && pit[2] == (TChar)'-' && pit[3] == (TChar)'-';
}
template <typename TChar>
inline bool IsCommentEnd(const TChar *pit) noexcept
{
   return pit[0] == (TChar)'-' && pit[1] == (TChar)'-' && pit[2] == (TChar)'>';
}

// Erases pointers pointing to positions inside comments, so that each comment is exactly one token.
template <typename TChar>
void RemoveInsideComments(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   auto it_erase_from = tokens->end();
   auto it_erase_to   = tokens->end();

   while (it_right != tokens->end()) {
      const TChar *pend = *it_right;
      for (const TChar *pit = *it_left; pit < pend; ++pit) {
         if (it_erase_from == tokens->end() && IsCommentStart(pit)) {
            it_erase_from = it_right; // erase next token
            pit += 3;                 // IsCommentStart() checks next 3 symbols
         }
         else if (it_erase_from != tokens->end() && IsCommentEnd(pit)) {
            it_erase_to = it_right; // erase untill (but not inclusive) next token
            break;
         }
      }

      if (it_erase_from != tokens->end() && it_erase_to != tokens->end()) {
         tokens->erase(it_erase_from, it_erase_to); // no-op if it_erase_from==it_erase_to
         it_right = it_erase_to;
         it_left  = it_right++;

         it_erase_from = tokens->end();
         it_erase_to   = tokens->end();
      }
      else {
         ++it_right;
         ++it_left;
      }
   }
}

enum Token
{
   OPEN    = 0x01, // opening xml tag
   CLOSE   = 0x02, // closing xml tag
   CONTENT = 0x04, // free text between opening and closing tags
   COMMENT = 0x08, // everything inside <!--  -->
   ERROR   = 0x00
};

// Determines type of the token whose first symbol is *pbegin and last symbol is *(pend-1).
template <typename TChar>
int DetermineToken(const TChar *pbegin, const TChar *pend) noexcept
{
   if (*pbegin == (TChar)'<') {

      if (*(pbegin + 1) == (TChar)'/') {
         return Token::CLOSE;
      }
      if (IsCommentStart(pbegin)) {
         return Token::COMMENT;
      }

      pbegin = std::find(pbegin, pend, (TChar)'>');
      if (pbegin == pend) {
         return Token::ERROR;
      }
      int what = Token::OPEN;
      if (*(pbegin - 1) == (TChar)'/')
         what |= Token::CLOSE;
      return what;
   }
   return Token::CONTENT;
}

// Removes comment tokens in the beginning of the list
template <typename TChar>
inline void RemoveLeadingComments(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   while (DetermineToken(*it_left, *it_right) == Token::COMMENT) {
      ++it_right;
      ++it_left;
      tokens->pop_front();
   }
}

// Reads element name from the opening tag starting at pbegin (it must point to a '<').
template <typename TChar>
std::basic_string<TChar> ExtractName(const TChar *pbegin, const TChar *pend)
{
   pend = std::find_if(++pbegin, pend, [](TChar c) { return IsSpace(c) || c == (TChar)'>' || c == (TChar)'/'; });
   return std::basic_string<TChar>(pbegin, pend);
}

// Reads attribute pairs from the tag starting at pbegin (it must point to a '<').
template <typename TChar>
std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> ExtractAttributes(const TChar *pbegin,
                                                                                         const TChar *pend)
{
   std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> attrs;
   // skip element name
   pbegin = std::find_if(pbegin, pend, [](TChar c) { return c == (TChar)'>' || IsSpace(c); });

   while (pbegin < pend) {
      const TChar *keybegin = std::find_if(pbegin, pend, IsAlpha<TChar>);
      if (keybegin == pend) {
         return attrs;
      }
      const TChar *keyend   = std::find(keybegin, pend, (TChar)'=');
      const TChar *valbegin = keyend + 2;
      const TChar *valend   = std::find(valbegin, pend, *(valbegin - 1)); // either " or '

      std::basic_string<TChar> key(keybegin, keyend);
      std::basic_string<TChar> value(valbegin, valend);
      attrs.emplace(std::move(key), std::move(value));
      pbegin = valend;
   }
   return attrs;
}

// Checks whether 'from' points to start of an entity reference. If so, returns a pointer to the
// substitution string and writes the length of the entity reference to 'count'. Returns nullptr if no
// entity reference at 'from'.
template <typename TChar>
const TChar *CheckEntityRef(const TChar *from, std::size_t *count, std::size_t er_index)
{
   const TChar **table_line = EntityRefTable<TChar>(er_index);
   for (int col = 0; col < 3; ++col) {
      // Compare with all 3 representations
      const TChar *word = table_line[col];
      const TChar *pit  = from;

      bool equal = true;
      while (*word && *pit) {
         if (*word++ != *pit++) {
            equal = false;
            break;
         }
      }
      if (equal) {
         *count = pit - from;
         return table_line[3];
      }
   }
   return nullptr;
}

// Replaces all entity references in 'content' by the corresponding ascii symbols.
template <typename TChar>
void SubstituteEntityRef(std::basic_string<TChar> *content)
{
   if (content->empty()) {
      return;
   }
   size_t count = 0;
   for (const TChar *from = &content->front(); from < &content->back() - 2; ++from) {
      // Compare 'from' with all 5 entity references
      for (int er_index = 0; er_index < 5; ++er_index) {
         const TChar *repl_str = CheckEntityRef(from, &count, er_index);
         if (repl_str) {
            std::size_t pos = from - &content->front();
            content->replace(pos, count, repl_str); // no better overload :(
            from = &content->front() + pos;
            break;
         }
      }
   }
}

// Replaces unallowed symbols in 'content' by entity references (uses first column)
template <typename TChar>
std::basic_string<TChar> InsertEntityRef(std::basic_string<TChar> content)
{
   for (auto it = content.begin(); it != content.end(); ++it) {
      for (int er_index = 0; er_index < 5; ++er_index) {
         const TChar **table_line = EntityRefTable<TChar>(er_index);

         if (*it == *table_line[3]) { // comparing only one char
            std::size_t pos = it - content.begin();
            content.replace(pos, 1, table_line[0]); // uses first column for substitution
            it = content.begin() + pos + 3;         // min length of ER is 4 = 3+1
            break;
         }
      }
   }
   return std::move(content);
}

// Node in the resulting tree. Contains all data about one xml element and pointers to its children.
template <typename TChar>
struct ElementData
{
   typedef TChar char_t;
   typedef std::basic_string<char_t> string_t;
   typedef ElementData<char_t> my_t;

   ElementData() = default;
   ElementData(const string_t &name, const string_t &content, const std::unordered_map<string_t, string_t> &attrs)
       : name(name), content(content), attrs(attrs)
   {}
   std::unique_ptr<my_t> Copy() const
   {
      auto pcopy = std::make_unique<my_t>(name, content, attrs);
      for (const auto &pchild : children) {
         auto pchild_copy = pchild->Copy();
         pcopy->children.emplace_back(std::move(pchild_copy));
      }
      return std::move(pcopy);
   }

   string_t name;
   string_t content;
   std::unordered_map<string_t, string_t> attrs;
   std::vector<std::unique_ptr<my_t>> children;
};

template <typename TChar>
std::basic_ostream<TChar> &operator<<(std::basic_ostream<TChar> &out, const ElementData<TChar> &e)
{
   out << MarkupTable<TChar>(Markup::OPENING_TAG_START) << e.name;
   for (const auto &attr : e.attrs) {
      out << MarkupTable<TChar>(Markup::ATTR_START) << attr.first << MarkupTable<TChar>(Markup::ATTR_MID) << attr.second
          << MarkupTable<TChar>(Markup::ATTR_END);
   }
   if (e.content.empty() && e.children.empty()) {
      return out << MarkupTable<TChar>(Markup::SINGLE_TAG_END);
   }
   out << MarkupTable<TChar>(Markup::OPENING_TAG_END);
   out << InsertEntityRef(e.content);
   for (const auto &pchild : e.children) {
      out << *pchild;
   }
   out << MarkupTable<TChar>(Markup::CLOSING_TAG_START) << e.name << MarkupTable<TChar>(Markup::CLOSING_TAG_END);
   return out;
}

// Builds the element tree and returns pointer to its root. Declaration token must be removed from
// 'tokens' prior to calling this function. Ignores the rest after the root element has been closed.
template <typename TChar>
std::unique_ptr<ElementData<TChar>> BuildElementTree(const std::list<const TChar *> &tokens, bool replace_er)
{
   // Create stack and iterators
   std::stack<ElementData<TChar> *, std::list<ElementData<TChar> *>> tree;

   auto it_right = tokens.begin();
   auto it_left  = it_right++;

   // Set up root and push on stack
   auto root   = std::make_unique<ElementData<TChar>>();
   root->name  = ExtractName(*it_left, *it_right);
   root->attrs = ExtractAttributes(*it_left, *it_right);
   tree.push(root.get());

   ++it_right;
   ++it_left;

   // Last pointer in 'tokens' points to \0 so checking it_right instead of it_left
   for (; it_right != tokens.cend() && tree.size() > 0; ++it_right, ++it_left) {
      const TChar *pbegin = *it_left;
      const TChar *pend   = *it_right;

      int what = DetermineToken(pbegin, pend);

      if (what & Token::OPEN) {
         // Create and anchor a new element
         ElementData<TChar> *pelem = new ElementData<TChar>();
         tree.top()->children.emplace_back(pelem); // creates std::unique_ptr implicitly

         pelem->name  = ExtractName(pbegin, pend);
         pelem->attrs = ExtractAttributes(pbegin, pend);

         tree.push(pelem);
      }
      if (what & Token::CLOSE) {
         tree.pop();
         continue;
      }
      if (what == Token::CONTENT) {
         tree.top()->content.append(pbegin, pend);
         if (replace_er) {
            SubstituteEntityRef(&tree.top()->content);
         }
         continue;
      }
      if (what == Token::ERROR) {
         return nullptr;
      }
      // if (what == Token::COMMENT) continue;
   }
   return root;
}

} // namespace details


class Exception : public std::invalid_argument
{
public:
   Exception(const char *what) : std::invalid_argument(what)
   {}
   Exception(const std::string &what) : std::invalid_argument(what)
   {}
   virtual const char *what() const noexcept
   {
      return std::invalid_argument::what();
   }
};

// Thin wrapper containing pointer to a node in the element tree, and defining user interface
// functions to access and modify data. Has no ownership of the underlying node.
template <typename TChar>
class Element
{
public:
   typedef TChar char_t;
   typedef Element<char_t> my_t;

   Element(details::ElementData<char_t> *pdata) : pdata_(pdata)
   {
      if (!pdata_) {
         throw Exception("Failed to create element");
      }
   }

   const std::basic_string<char_t> &GetName() const noexcept
   {
      return pdata_->name;
   }
   // Set name that (optionally) includes namespace
   void SetName(std::basic_string<char_t> name)
   {
      pdata_->name = std::move(name);
   }
   // Set namespace and name
   void SetName(std::basic_string<char_t> ns, std::basic_string<char_t> name)
   {
      pdata_->name = std::move(ns) + ":" + std::move(name);
   }
   // Namespace name or empty.
   std::basic_string<char_t> GetNamePrefix() const
   {
      size_t pos = pdata_->name.find_first_of((char_t)':');
      if (pos != pdata_->name.npos) {
         return pdata_->name.substr(0, pos);
      }
      return "";
   }
   // Returns copy of the whole name if no namespace prefix.
   std::basic_string<char_t> GetNamePostfix() const
   {
      size_t pos = pdata_->name.find_first_of((char_t)':');
      if (pos != pdata_->name.npos) {
         return pdata_->name.substr(pos + 1);
      }
      return pdata_->name;
   }

   const std::basic_string<char_t> &GetContent() const noexcept
   {
      return pdata_->content;
   }
   void SetContent(std::basic_string<char_t> content)
   {
      if (GetChildCount() != 0)
         throw Exception("Cannot have both content and children");
      pdata_->content = std::move(content);
   }

   const std::basic_string<char_t> &GetAttributeValue(const std::basic_string<char_t> &attribute) const
   {
      auto it = pdata_->attrs.find(attribute);
      if (it == pdata_->attrs.cend()) {
         throw Exception("Attribute " + attribute + " not found");
      }
      return it->second;
   }
   const std::basic_string<char_t> &GetAttributeName(std::size_t index) const
   {
      return GetAttr(index).first;
   }
   const std::basic_string<char_t> &GetAttributeValue(std::size_t index) const
   {
      return GetAttr(index).second;
   }
   // Changes value of an existing attribute if 'name' is already in the list of attributes
   void AddAttribute(std::basic_string<char_t> name, std::basic_string<char_t> value)
   {
      pdata_->attrs[std::move(name)] = std::move(value);
   }

   std::size_t GetAttributeCount() const noexcept
   {
      return pdata_->attrs.size();
   }
   std::size_t GetChildCount() const noexcept
   {
      return pdata_->children.size();
   }

   // Creates wrapper for a child element and returns it.
   const my_t GetChild(std::size_t index) const
   {
      if (index >= pdata_->children.size()) {
         throw Exception("Child " + std::to_string(index) +
                         " not found, child count = " + std::to_string(pdata_->children.size()));
      }
      details::ElementData<char_t> *pnode = pdata_->children[index].get();
      return pnode;
   }
   const my_t GetChild(const std::basic_string<char_t> &name) const
   {
      for (auto it = pdata_->children.cbegin(); it != pdata_->children.cend(); ++it) {
         details::ElementData<char_t> *pnode = it->get();
         if (pnode->name == name) {
            return pnode;
         }
      }
      throw Exception("Child " + name + " not found");
   }
   // Create a new child at pos. If 'pos' is larger than current children count, inserts child at the end
   my_t AddChild(std::size_t pos, const char_t *name = nullptr)
   {
      if (!pdata_->content.empty())
         throw Exception("Cannot have both content and children");

      auto it = pdata_->children.begin();
      while (it != pdata_->children.end() && pos > 0) {
         ++it;
         --pos;
      }
      details::ElementData<char_t> *pchild = new details::ElementData<char_t>();
      if (name)
         pchild->name = name;

      pdata_->children.insert(it, std::unique_ptr<details::ElementData<char_t>>(pchild));
      return pchild;
   }
   // Create new child at the end
   my_t AddChild(const char_t *name = nullptr)
   {
      if (!pdata_->content.empty())
         throw Exception("Cannot have both content and children");

      details::ElementData<char_t> *pchild = new details::ElementData<char_t>();
      if (name)
         pchild->name = name;

      pdata_->children.emplace_back(pchild);
      return pchild;
   }

   friend std::basic_ostream<char_t> &operator<<(std::basic_ostream<char_t> &out, const my_t &e);

private:
   const std::pair<const std::basic_string<char_t>, std::basic_string<char_t>> &GetAttr(std::size_t index) const
   {
      auto it = pdata_->attrs.cbegin();
      for (std::size_t i = 0; i < index; ++i) {
         if (it == pdata_->attrs.cend()) {
            throw Exception("Attribute " + std::to_string(index) + " not found");
         }
         ++it;
      }
      return *it;
   }
   details::ElementData<char_t> *pdata_;
};

template <typename TChar>
std::basic_ostream<TChar> &operator<<(std::basic_ostream<TChar> &out, const Element<TChar> &e)
{
   out << *e.pdata_;
   return out;
}

// Represents the whole xml document with (or without) declaration and one element tree.
template <typename TChar>
class Document
{
public:
   typedef TChar char_t;
   typedef Document<char_t> my_t;

   // Parse 'text'
   Document(const char_t *text, bool replace_er)
   {
      std::list<const char_t *> tokens = details::Tokenize(text);
      details::RemoveGaps(&tokens);
      details::RemoveInsideComments(&tokens);
      details::RemoveLeadingComments(&tokens);

      const char_t *pfirst = *tokens.begin();
      if (*pfirst != (char_t)'<') {
         throw Exception("Malformed beginning");
      }
      if (*(pfirst + 1) == (char_t)'?') { // has declaration
         auto declaration = details::ExtractAttributes(pfirst, *(++tokens.begin()));

         const std::basic_string<char_t> *decl_attrs = details::DeclarationAttrs<char_t>();
         std::basic_string<char_t> *decl_data[]      = {&version_, &encoding_, &standalone_};

         for (int i = 0; i < 3; ++i) {
            auto it = declaration.find(decl_attrs[i]);
            if (it != declaration.cend()) {
               *(decl_data[i]) = it->second;
            }
         }
         tokens.pop_front();
         details::RemoveLeadingComments(&tokens);
      }
      proot_ = details::BuildElementTree(tokens, replace_er);
      if (!proot_) {
         throw Exception("Malformed xml");
      }
   }
   // Create new empty document
   Document(std::basic_string<char_t> root_name, std::basic_string<char_t> version, std::basic_string<char_t> encoding,
            std::basic_string<char_t> standalone)
       : proot_(std::make_unique<details::ElementData<char_t>>()), version_(std::move(version)),
         encoding_(std::move(encoding)), standalone_(std::move(standalone))
   {
      proot_->name = std::move(root_name);
   }

   Document(my_t &&) = default;
   my_t &operator=(my_t &&) = default;

   Document(const my_t &) = delete;
   my_t &operator=(const my_t &) = delete;

   // Create a deep non-const copy of the document
   std::unique_ptr<my_t> Copy() const
   {
      auto pcopy     = std::make_unique<my_t>(std::basic_string<char_t>(), version_, encoding_, standalone_);
      auto root_copy = proot_->Copy();
      pcopy->proot_  = std::move(root_copy);
      return pcopy;
   }
   // Serialize to xml
   std::basic_string<char_t> ToString() const
   {
      using details::Markup;
      using details::MarkupTable;

      std::basic_ostringstream<char_t> out;
      if (!(version_.empty() && encoding_.empty() && standalone_.empty())) {
         out << MarkupTable<char_t>(Markup::DECL_START);

         const std::basic_string<char_t> *decl_attrs  = details::DeclarationAttrs<char_t>();
         const std::basic_string<char_t> *decl_data[] = {&version_, &encoding_, &standalone_};
         for (int i = 0; i < 3; ++i) {
            if (!decl_data[i]->empty())
               out << MarkupTable<char_t>(Markup::ATTR_START) << decl_attrs[i] << MarkupTable<char_t>(Markup::ATTR_MID)
                   << *(decl_data[i]) << MarkupTable<char_t>(Markup::ATTR_END);
         }
         out << MarkupTable<char_t>(Markup::DECL_END);
      }
      out << *proot_;
      return out.str();
   }

   const std::basic_string<char_t> &GetVersion() const noexcept
   {
      return version_;
   }
   void SetVersion(const char_t *version)
   {
      version_ = version;
   }

   const std::basic_string<char_t> &GetEncoding() const noexcept
   {
      return encoding_;
   }
   void SetEncoding(const char_t *encoding)
   {
      encoding_ = encoding;
   }

   const std::basic_string<char_t> &GetStandalone() const noexcept
   {
      return standalone_;
   }
   void SetStandalone(const char_t *standalone)
   {
      standalone_ = standalone;
   }

   const Element<char_t> GetRoot() const noexcept
   {
      return proot_.get();
   }
   Element<char_t> GetRoot() noexcept
   {
      return proot_.get();
   }

private:
   std::unique_ptr<details::ElementData<char_t>> proot_;
   std::basic_string<char_t> version_;
   std::basic_string<char_t> encoding_;
   std::basic_string<char_t> standalone_;
};

// New blank document without header
template <typename TChar>
inline std::unique_ptr<Document<TChar>> NewDocument(const TChar *root_name)
{
   return std::make_unique<Document<TChar>>(root_name, std::basic_string<TChar>(), std::basic_string<TChar>(),
       std::basic_string<TChar>());
}

// New blank document with header
template <typename TChar>
inline std::unique_ptr<Document<TChar>> NewDocument(const TChar *root_name, const TChar *version, const TChar *encoding,
                                                    const TChar *standalone)
{
   return std::make_unique<Document<TChar>>(root_name, version, encoding, standalone);
}

// Creates xml::Document that reads and parses 'text'. Parsing entity references might slow down the
// process, set entity_references to 'false' if that is undesirable.
template <typename TChar>
inline std::unique_ptr<const Document<TChar>> ParseString(const TChar *text, bool entity_references = true)
{
   return std::make_unique<const Document<TChar>>(text, entity_references);
}

// Creates xml::Document that reads and parses 'text'. Parsing entity references might slow down the
// process, set entity_references to 'false' if that is undesirable.
template <typename TChar>
inline std::unique_ptr<const Document<TChar>> ParseString(const std::basic_string<TChar> &text,
                                                          bool entity_references = true)
{
   return std::make_unique<const Document<TChar>>(text.c_str(), entity_references);
}

// Reads data from 'stream' into cache and parses it into an xml::Document. Lower performance than
// the other overloads. Parsing entity references might slow down the process, set entity_references
// to 'false' if that is undesirable.
template <typename TChar>
std::unique_ptr<const Document<TChar>> ParseStream(std::basic_istream<TChar> &stream, bool entity_references = true)
{
   constexpr std::size_t SIZE = 4096;
   std::basic_string<TChar> s;
   TChar buf[SIZE];

   while (stream.read(buf, SIZE))
      s.append(buf, SIZE);
   s.append(buf, stream.gcount());

   return std::make_unique<const Document<TChar>>(s.c_str(), entity_references);
}

} // namespace xml

#endif // XMLPARSER_HPP