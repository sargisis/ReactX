// A kanban board with drag and drop, one source for the browser and the
// terminal: drag a card with the mouse (or a finger) to another column, or
// focus it (Tab) and move it with the Left / Right arrows.
//
// The pointer events are the same in both: a card's onPointerDown starts the
// drag, a column's onMouseEnter marks where it would land, the column's
// onPointerUp drops it. No pointerCapture here: the release must reach the
// column under the pointer, not the card.
//
//   native:  ./build/debug/kanban            (mouse in the terminal)
//   web:     build/web/web_kanban.html
#include <string>
#include <vector>

#include "reactx/reactx.hpp"
#ifdef __EMSCRIPTEN__
#include "reactx/dom.hpp"
#else
#include "reactx/terminal.hpp"
#endif

using namespace rx;

namespace {

// The two backends draw different tags; layout props the other one doesn't
// know are ignored there (the terminal ignores style, the page ignores border).
#ifdef __EMSCRIPTEN__
constexpr const char* kBox = "div";
constexpr const char* kText = "span";
#else
constexpr const char* kBox = "box";
constexpr const char* kText = "text";
#endif

struct Card {
    int id = 0;
    std::string title;
    bool operator==(const Card&) const = default;
};
using Board = std::vector<std::vector<Card>>;

const std::vector<std::string> kColumns = {"To do", "Doing", "Done"};

#ifdef __EMSCRIPTEN__
// The page's look (the terminal draws borders and colors from props instead).
constexpr const char* kCss = R"css(
#app { max-width: 820px; }
.kanban { display: flex; flex-direction: column; gap: 12px; user-select: none; }
.kanban .hint { color: var(--muted); font-size: 0.9rem; text-align: center; }
.board { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; }
.column { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 10px;
          display: flex; flex-direction: column; gap: 8px; min-height: 220px; transition: border-color .15s, background .15s; }
.column.target { border-color: var(--accent); background: color-mix(in srgb, var(--accent) 8%, var(--card)); }
.column h2 { font-size: 0.85rem; margin: 2px 4px 4px; color: var(--muted); font-weight: 600; }
.card { padding: 10px 12px; border-radius: 9px; border: 1px solid var(--border); background: var(--bg);
        cursor: grab; touch-action: none; }
.card:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }
.card.lifted { cursor: grabbing; border-color: var(--accent); box-shadow: 0 6px 18px rgb(0 0 0 / 0.15); transform: rotate(-1.5deg); }
.empty { color: var(--muted); font-size: 0.85rem; text-align: center; padding: 16px 0; }
@media (max-width: 560px) { .board { grid-template-columns: 1fr; } .column { min-height: 0; } }
)css";
#endif

// Moves card `id` to the end of column `to`.
Board moved(Board board, int id, int to) {
    for (auto& column : board)
        for (auto it = column.begin(); it != column.end(); ++it)
            if (it->id == id) {
                Card card = *it;
                column.erase(it);
                board[static_cast<std::size_t>(to)].push_back(card);
                return board;
            }
    return board;
}

int columnOf(const Board& board, int id) {
    for (std::size_t c = 0; c < board.size(); ++c)
        for (const Card& card : board[c])
            if (card.id == id) return static_cast<int>(c);
    return -1;
}

Element App(const Props&) {
    auto [board, setBoard] = useState(Board{
        {{1, "Pointer events"}, {2, "Drag and drop"}, {3, "Focus from code"}},
        {{4, "useNodeRef"}},
        {{5, "renderToString"}, {6, "Chart.js demo"}},
    });
    auto [dragging, setDragging] = useState(0);  // card id, 0 = none
    auto [over, setOver] = useState(-1);         // the column under the pointer

    auto drop = [board, dragging, setBoard, setDragging](int column) {
        if (dragging && columnOf(board, dragging) != column) setBoard(moved(board, dragging, column));
        setDragging(0);
    };

    std::vector<Element> columns;
    for (std::size_t c = 0; c < board.size(); ++c) {
        int col = static_cast<int>(c);
        bool target = dragging && over == col && columnOf(board, dragging) != col;
        std::vector<Element> cards;
        for (const Card& card : board[c]) {
            int id = card.id;
            bool lifted = dragging == id;
            cards.push_back(h(kBox, {
                {"key", std::to_string(id)},
                {"className", lifted ? "card lifted" : "card"},
                {"focusable", true}, {"tabindex", 0},
                {"color", lifted ? "cyan" : ""}, {"bold", lifted},
                {"onPointerDown", [setDragging, id](const PointerEvent& e) { if (e.button == 0) setDragging(id); }},
                {"onKey", [board, setBoard, id, col](const KeyEvent& k) {
                     int to = k.key == "Right" ? col + 1 : k.key == "Left" ? col - 1 : -1;
                     if (to >= 0 && to < static_cast<int>(board.size())) setBoard(moved(board, id, to));
                 }},
            }, {h(kText, {}, {(lifted ? "» " : "  ") + card.title})}));
        }
        if (cards.empty()) cards.push_back(h(kText, {{"key", "empty"}, {"className", "empty"}, {"dim", true}}, {"  drop here"}));
        columns.push_back(h(kBox, {
            {"key", kColumns[c]},
            {"className", target ? "column target" : "column"},
#ifndef __EMSCRIPTEN__
            {"border", true}, {"title", kColumns[c] + " (" + std::to_string(board[c].size()) + ")"},
#endif
            {"flexGrow", 1}, {"padding", 0}, {"color", target ? "yellow" : ""},
            {"onMouseEnter", [setOver, col] { setOver(col); }},
            {"onPointerUp", [drop, col](const PointerEvent&) { drop(col); }},
        }, {
#ifdef __EMSCRIPTEN__
            h("h2", {}, {kColumns[c] + " · " + std::to_string(board[c].size())}),
#endif
            fragment(std::move(cards)),
        }));
    }

    return h(kBox, {{"className", "kanban"}}, {
#ifdef __EMSCRIPTEN__
        h("style", {}, {kCss}),
#endif
        h(kText, {{"className", "hint"}, {"dim", true}},
          {dragging ? "drop it on a column" : "drag a card, or Tab to a card and use ← →"}),
        // A release outside every column, or leaving the board, cancels the drag.
        h(kBox, {{"className", "board"}, {"direction", "row"}, {"gap", 1}, {"flexGrow", 1},
                 {"onPointerUp", [setDragging](const PointerEvent&) { setDragging(0); }},
                 {"onMouseLeave", [setDragging, setOver] { setDragging(0); setOver(-1); }}},
          std::move(columns)),
    });
}

}  // namespace

int main() {
#ifdef __EMSCRIPTEN__
    mountDom(h(App));
#else
    TerminalApp app;
    app.run(h(App));
#endif
}
