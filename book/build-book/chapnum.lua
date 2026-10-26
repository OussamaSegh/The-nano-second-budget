-- Heading labels for the print and EPUB editions.
--   ## ... {#ch05}  -> "Chapter 5" label, class chapter
--   ## ... {#f03}   -> "Chapter F3" label (Part 0 Foundations)
--   ## Appendix A. Title {#appA} -> "Appendix A" label + "Title"
--   # Part IV. Title {.part}     -> "Part IV" label + "Title"
-- Every chapter-level heading gets data-short (its bare title) for running heads.
local function split_label(inlines, pattern)
  local text = pandoc.utils.stringify(inlines)
  local label, rest = text:match(pattern)
  return label, rest
end

local chapter_key = nil
local fig_count = 0

function Figure(fig)
  if not chapter_key then return nil end
  fig_count = fig_count + 1
  local label = "Figure " .. chapter_key .. "." .. fig_count
  local blocks = fig.caption.long
  if #blocks > 0 and blocks[1].content then
    blocks[1].content:insert(1, pandoc.Space())
    blocks[1].content:insert(1, pandoc.Strong({pandoc.Str(label)}))
  end
  fig.caption.long = blocks
  if fig.identifier == "" then fig.identifier = "fig-" .. chapter_key .. "-" .. fig_count end
  return fig
end

function Header(h)
  if h.level == 1 and h.classes:includes("part") then
    local label, rest = split_label(h.content, "^(Part [%w]+)%.%s+(.*)$")
    if label then
      h.content = pandoc.Inlines({pandoc.Span({pandoc.Str(label)}, {class = "partnum"}),
                                  pandoc.Space(), pandoc.Span(pandoc.Inlines(rest), {class = "parttitle"})})
      h.attributes["data-short"] = rest
    else
      h.attributes["data-short"] = pandoc.utils.stringify(h.content)
    end
    return h
  end
  if h.level == 1 and h.classes:includes("unnumbered") then
    h.attributes["data-short"] = pandoc.utils.stringify(h.content)
    h.classes:insert("frontchapter")
    return h
  end
  if h.level == 2 then
    local label = nil
    local n = h.identifier:match("^ch(%d+)$")
    if n then label = "Chapter " .. tonumber(n); chapter_key = tostring(tonumber(n)); fig_count = 0 end
    local f = h.identifier:match("^f(%d+)$")
    if f then label = "Chapter F" .. tonumber(f); chapter_key = "F" .. tonumber(f); fig_count = 0 end
    if h.identifier:match("^app%u$") then
      local l, rest = split_label(h.content, "^(Appendix %u)%.%s+(.*)$")
      if l then label = l; h.content = pandoc.Inlines(rest) end
    end
    if label then
      h.attributes["data-short"] = pandoc.utils.stringify(h.content)
      h.content:insert(1, pandoc.Span({pandoc.Str(label)}, {class = "chapnum"}))
      h.content:insert(2, pandoc.Space())
      h.classes:insert("chapter")
    elseif h.identifier == "references" then
      h.attributes["data-short"] = "References"
      h.classes:insert("chapter")
    end
  end
  return h
end
