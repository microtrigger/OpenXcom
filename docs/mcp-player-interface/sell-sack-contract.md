# Bounded Sell/Sack interface

Schema 31 adds `open_sell` from Basescape and a `sell` presentation
of rendered title, funds, sales total, store-space text, column headings,
category and visible rows. Row handles identify only the current presentation;
no item IDs, internal item types, hidden eligibility or calculated capacities
are published. The category dropdown uses the ordinary visible-list contract.

`change_sale_quantity` changes one advertised row by one step through its
ordinary quantity handler. Draft quantities are not committed sales.
`accept_sell` uses the ordinary Sell/Sack handler; `cancel_sell` discards the
draft. `scroll_sell` moves one visible list step. These controls remain subject
to freshness, shown-widget, paint, focus and real-overlay checks. Native rules
still decide the outcome, which must be established from fresh visible state.

This slice does not claim every category, overflow, craft, personnel dismissal,
mod or list-navigation combination. In particular, quantity editing for a
personnel row is distinct from committing a dismissal. No full Sell/Sack
acceptance or full player-equivalent interface completion is implied.

Focused native provider/widget, closed-protocol and adapter checks pass, as
does the production build. Native tests include synthetic economy seams and
are not accounting proof. Short isolated live tests in both UFO and TFTD
confirmed open/Cancel, category selection, personnel draft increment/decrement
without dismissal, one-step scrolling, and sale of one expendable item. Fresh
visible stock, funds and cleared draft after reopening established the sale.
Gameplay used MCP with passive video and no visual gameplay fallback.

Some row name/value cells remain unavailable rather than exposing unverified
text. This presentation gap remains open; shown totals and funds do not make
all individual cells qualified. The wider regression suite is not fully passing.
