# Visible UFOPEDIA / UFOPAEDIA directory

Geoscape exposes `open_ufopaedia` only through its currently available painted
UFOPEDIA/UFOPAEDIA button. The native modal pauses the underlying campaign.

`ufopaedia_start` projects its title, currently painted category-button labels
as observation-local single-cell rows, scroll directions and ordinary close
button. `select_ufopaedia_category` accepts a currently offered row handle;
`close_ufopaedia` returns through the ordinary handler.

`ufopaedia_select` projects the native known-article list in the same bounded
shape. `select_ufopaedia_article` invokes native article selection and enters
the existing article interface; `close_ufopaedia_select` returns to categories.
Both directory screens use `scroll_visible_list` only in offered directions.
No category or article identifiers, hidden list tails, requirements, unlock
predictions or full mod catalogs are exported. Rows use painted labels, including
nullable unavailable text; handles are not persistent identities. Article-list
rows also report their painted selection highlight, including the retained
highlight after closing an article. Category-button rows remain unselected.

Article availability remains controlled by the game's existing `Ufopaedia::list`
research/availability filtering. A visible category may lead to an empty list;
empty lists still permit ordinary return but confer no selection authority.
Current-state, paint, clipping, overlay and fresh-observation guards remain in
force. Directory reads do not unlock research, mutate the campaign, or grant
controls for hidden screens. This contract applies to both UFO and TFTD; it does
not establish every article layout or complete-project qualification.
