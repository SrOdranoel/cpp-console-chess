// Graphical chess (Win32 + GDI+): full rules (castling, en passant, promotion, check/mate/stalemate) with a simple AI.
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace Gdiplus {
using std::max;
using std::min;
} // namespace Gdiplus
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

struct Move {
    int from, to;
    char promo; // 0 or one of Q,R,B,N
};

struct Position {
    char b[64]; // index = rank * 8 + file, rank 0 is White's back rank
    bool white = true;
    bool cK = true, cQ = true, ck = true, cq = true;
    int ep = -1;
    int half = 0;
};

static inline int F(int s) { return s & 7; }
static inline int R(int s) { return s >> 3; }
static inline bool in(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }
static inline bool isWhitePiece(char c) { return c != '.' && std::isupper((unsigned char)c); }
static inline bool isBlackPiece(char c) { return c != '.' && std::islower((unsigned char)c); }
static inline char up(char c) { return (char)std::toupper((unsigned char)c); }

static const int KN[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
static const int DIRS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

Position initial() {
    Position p;
    const char* back = "RNBQKBNR";
    for (int i = 0; i < 64; i++) p.b[i] = '.';
    for (int f = 0; f < 8; f++) {
        p.b[f] = back[f];
        p.b[8 + f] = 'P';
        p.b[48 + f] = 'p';
        p.b[56 + f] = (char)std::tolower(back[f]);
    }
    return p;
}

bool attacked(const Position& p, int sq, bool byWhite) {
    int f = F(sq), r = R(sq);
    int pr = byWhite ? r - 1 : r + 1;
    char pawn = byWhite ? 'P' : 'p';
    for (int df = -1; df <= 1; df += 2)
        if (in(f + df, pr) && p.b[pr * 8 + f + df] == pawn) return true;

    char knight = byWhite ? 'N' : 'n';
    char king = byWhite ? 'K' : 'k';
    for (auto& k : KN)
        if (in(f + k[0], r + k[1]) && p.b[(r + k[1]) * 8 + f + k[0]] == knight) return true;
    for (int d = 0; d < 8; d++) {
        int nf = f + DIRS[d][0], nr = r + DIRS[d][1];
        if (in(nf, nr) && p.b[nr * 8 + nf] == king) return true;
        while (in(nf, nr)) {
            char c = p.b[nr * 8 + nf];
            if (c != '.') {
                if (isWhitePiece(c) == byWhite) {
                    char t = up(c);
                    if (t == 'Q' || (d < 4 && t == 'R') || (d >= 4 && t == 'B')) return true;
                }
                break;
            }
            nf += DIRS[d][0];
            nr += DIRS[d][1];
        }
    }
    return false;
}

bool inCheck(const Position& p, bool white) {
    char k = white ? 'K' : 'k';
    for (int i = 0; i < 64; i++)
        if (p.b[i] == k) return attacked(p, i, !white);
    return false;
}

static void addMove(std::vector<Move>& out, const Position& p, int from, int to) {
    bool pawn = up(p.b[from]) == 'P';
    int promoRank = p.white ? 7 : 0;
    if (pawn && R(to) == promoRank) {
        for (char c : {'Q', 'R', 'B', 'N'}) out.push_back({from, to, c});
    } else {
        out.push_back({from, to, 0});
    }
}

std::vector<Move> pseudoMoves(const Position& p) {
    std::vector<Move> out;
    bool w = p.white;
    for (int s = 0; s < 64; s++) {
        char c = p.b[s];
        if (c == '.' || isWhitePiece(c) != w) continue;
        char t = up(c);
        int f = F(s), r = R(s);
        if (t == 'P') {
            int dir = w ? 1 : -1;
            int startRank = w ? 1 : 6;
            if (in(f, r + dir) && p.b[(r + dir) * 8 + f] == '.') {
                addMove(out, p, s, (r + dir) * 8 + f);
                if (r == startRank && p.b[(r + 2 * dir) * 8 + f] == '.')
                    addMove(out, p, s, (r + 2 * dir) * 8 + f);
            }
            for (int df = -1; df <= 1; df += 2) {
                int nf = f + df, nr = r + dir;
                if (!in(nf, nr)) continue;
                int to = nr * 8 + nf;
                char tc = p.b[to];
                if ((tc != '.' && isWhitePiece(tc) != w) || to == p.ep) addMove(out, p, s, to);
            }
        } else if (t == 'N' || t == 'K') {
            if (t == 'N') {
                for (auto& k : KN) {
                    int nf = f + k[0], nr = r + k[1];
                    if (!in(nf, nr)) continue;
                    char tc = p.b[nr * 8 + nf];
                    if (tc == '.' || isWhitePiece(tc) != w) out.push_back({s, nr * 8 + nf, 0});
                }
            } else {
                for (auto& d : DIRS) {
                    int nf = f + d[0], nr = r + d[1];
                    if (!in(nf, nr)) continue;
                    char tc = p.b[nr * 8 + nf];
                    if (tc == '.' || isWhitePiece(tc) != w) out.push_back({s, nr * 8 + nf, 0});
                }
                int base = w ? 0 : 56;
                if (s == base + 4 && !attacked(p, s, !w)) {
                    bool ks = w ? p.cK : p.ck, qs = w ? p.cQ : p.cq;
                    char rook = w ? 'R' : 'r';
                    if (ks && p.b[base + 7] == rook && p.b[base + 5] == '.' && p.b[base + 6] == '.' &&
                        !attacked(p, base + 5, !w) && !attacked(p, base + 6, !w))
                        out.push_back({s, base + 6, 0});
                    if (qs && p.b[base] == rook && p.b[base + 1] == '.' && p.b[base + 2] == '.' &&
                        p.b[base + 3] == '.' && !attacked(p, base + 3, !w) && !attacked(p, base + 2, !w))
                        out.push_back({s, base + 2, 0});
                }
            }
        } else {
            int d0 = (t == 'B') ? 4 : 0;
            int d1 = (t == 'R') ? 4 : 8;
            for (int d = d0; d < d1; d++) {
                int nf = f + DIRS[d][0], nr = r + DIRS[d][1];
                while (in(nf, nr)) {
                    char tc = p.b[nr * 8 + nf];
                    if (tc == '.') {
                        out.push_back({s, nr * 8 + nf, 0});
                    } else {
                        if (isWhitePiece(tc) != w) out.push_back({s, nr * 8 + nf, 0});
                        break;
                    }
                    nf += DIRS[d][0];
                    nr += DIRS[d][1];
                }
            }
        }
    }
    return out;
}

Position apply(const Position& p, const Move& m) {
    Position n = p;
    char pc = n.b[m.from];
    char t = up(pc);
    char cap = n.b[m.to];
    n.b[m.to] = pc;
    n.b[m.from] = '.';
    if (t == 'P' && m.to == p.ep && F(m.to) != F(m.from)) n.b[R(m.from) * 8 + F(m.to)] = '.';
    if (t == 'K' && std::abs(F(m.to) - F(m.from)) == 2) {
        int r = R(m.from);
        if (F(m.to) == 6) {
            n.b[r * 8 + 5] = n.b[r * 8 + 7];
            n.b[r * 8 + 7] = '.';
        } else {
            n.b[r * 8 + 3] = n.b[r * 8];
            n.b[r * 8] = '.';
        }
    }
    if (m.promo) n.b[m.to] = p.white ? m.promo : (char)std::tolower(m.promo);
    n.ep = -1;
    if (t == 'P' && std::abs(R(m.to) - R(m.from)) == 2) n.ep = (m.from + m.to) / 2;
    if (pc == 'K') n.cK = n.cQ = false;
    if (pc == 'k') n.ck = n.cq = false;
    if (m.from == 0 || m.to == 0) n.cQ = false;
    if (m.from == 7 || m.to == 7) n.cK = false;
    if (m.from == 56 || m.to == 56) n.cq = false;
    if (m.from == 63 || m.to == 63) n.ck = false;
    n.half = (t == 'P' || cap != '.') ? 0 : p.half + 1;
    n.white = !p.white;
    return n;
}

std::vector<Move> legalMoves(const Position& p) {
    std::vector<Move> out;
    for (const Move& m : pseudoMoves(p))
        if (!inCheck(apply(p, m), p.white)) out.push_back(m);
    return out;
}

bool insufficientMaterial(const Position& p) {
    int minors = 0;
    for (int i = 0; i < 64; i++) {
        char t = up(p.b[i]);
        if (p.b[i] == '.' || t == 'K') continue;
        if (t == 'B' || t == 'N') minors++;
        else return false;
    }
    return minors <= 1;
}

std::string sqName(int s) {
    return std::string{(char)('a' + F(s)), (char)('1' + R(s))};
}

std::string moveName(const Move& m) {
    std::string s = sqName(m.from) + sqName(m.to);
    if (m.promo) s += (char)std::tolower(m.promo);
    return s;
}

// ---- AI ----

static int pieceValue(char t) {
    switch (t) {
        case 'P': return 100;
        case 'N': return 320;
        case 'B': return 330;
        case 'R': return 500;
        case 'Q': return 900;
        default: return 0;
    }
}

// Score from the side-to-move's point of view.
int evaluate(const Position& p) {
    int score = 0;
    for (int s = 0; s < 64; s++) {
        char c = p.b[s];
        if (c == '.') continue;
        char t = up(c);
        int v = pieceValue(t);
        int cf = std::abs(2 * F(s) - 7), cr = std::abs(2 * R(s) - 7); // distance from center
        if (t == 'P' || t == 'N' || t == 'B') v += (14 - cf - cr) * 3;
        if (t == 'P') v += (isWhitePiece(c) ? R(s) : 7 - R(s)) * 5;
        score += isWhitePiece(c) ? v : -v;
    }
    return p.white ? score : -score;
}

int negamax(const Position& p, int depth, int alpha, int beta) {
    if (p.half >= 100 || insufficientMaterial(p)) return 0;
    std::vector<Move> moves = legalMoves(p);
    if (moves.empty()) return inCheck(p, p.white) ? -100000 - depth : 0;
    if (depth == 0) return evaluate(p);
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return pieceValue(up(p.b[a.to])) > pieceValue(up(p.b[b.to]));
    });
    int best = INT_MIN + 1;
    for (const Move& m : moves) {
        int sc = -negamax(apply(p, m), depth - 1, -beta, -alpha);
        if (sc > best) best = sc;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

Move bestMove(const Position& p, int depth) {
    std::vector<Move> moves = legalMoves(p);
    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(moves.begin(), moves.end(), rng);
    Move best = moves[0];
    int bestScore = INT_MIN + 1;
    for (const Move& m : moves) {
        int sc = -negamax(apply(p, m), depth - 1, INT_MIN + 1, -bestScore);
        if (sc > bestScore) {
            bestScore = sc;
            best = m;
        }
    }
    return best;
}

// ---- GUI ----

using namespace Gdiplus;

enum Screen { SCR_MENU, SCR_GAME };
enum BtnId { B_WHITE, B_BLACK, B_TWO, B_LEVEL, B_QUIT, B_UNDO, B_FLIP, B_MENU };

struct Button {
    int id;
    Rect rc;
    std::wstring text;
};

static const int SQ = 80, BX = 40, BY = 40, BOARD = SQ * 8;
static const int PANEL_X = 710, PANEL_W = 250, CW = 990, CH = 720;
static const wchar_t* LEVELS[] = {L"Easy", L"Medium", L"Hard"};
static const int DEPTHS[] = {1, 2, 3};
static const UINT WM_AI_DONE = WM_APP + 1;

struct App {
    HWND hwnd = nullptr;
    Screen screen = SCR_MENU;
    int level = 2;
    int mode = 1; // 1 = human White, 2 = human Black, 3 = two players
    bool aiWhite = false, aiBlack = true;
    bool flipped = false;
    std::vector<Position> hist;
    std::vector<Move> played;
    std::vector<Move> legal;
    int sel = -1;
    bool dragging = false;
    POINT mouse{0, 0};
    bool thinking = false;
    int gameId = 0;
    bool promoPending = false;
    int pFrom = -1, pTo = -1;
    bool over = false, check = false;
    std::wstring result;
};
static App app;
static std::unique_ptr<GraphicsPath> g_glyph[6];

static std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ---- board geometry ----

static void squareOrigin(int s, int& x, int& y) {
    x = BX + (app.flipped ? 7 - F(s) : F(s)) * SQ;
    y = BY + (app.flipped ? R(s) : 7 - R(s)) * SQ;
}

static int squareAt(int px, int py) {
    if (px < BX || py < BY || px >= BX + BOARD || py >= BY + BOARD) return -1;
    int c = (px - BX) / SQ, r = (py - BY) / SQ;
    return (app.flipped ? r : 7 - r) * 8 + (app.flipped ? 7 - c : c);
}

static Rect promoRect(int i) { return Rect(BX + (BOARD - 390) / 2 + i * 100, BY + (BOARD - 90) / 2, 90, 90); }

static std::vector<Button> buttons() {
    std::vector<Button> v;
    if (app.screen == SCR_MENU) {
        const int w = 360, h = 52, x = (CW - w) / 2;
        int y = 335;
        v.push_back({B_WHITE, Rect(x, y, w, h), L"Play as White vs Computer"});
        y += 64;
        v.push_back({B_BLACK, Rect(x, y, w, h), L"Play as Black vs Computer"});
        y += 64;
        v.push_back({B_TWO, Rect(x, y, w, h), L"Two Players"});
        y += 64;
        v.push_back({B_LEVEL, Rect(x, y, w, h), std::wstring(L"Difficulty: ") + LEVELS[app.level]});
        y += 64;
        v.push_back({B_QUIT, Rect(x, y, w, h), L"Quit"});
    } else {
        v.push_back({B_UNDO, Rect(PANEL_X, 528, PANEL_W, 44), L"Undo"});
        v.push_back({B_FLIP, Rect(PANEL_X, 582, PANEL_W, 44), L"Flip board"});
        v.push_back({B_MENU, Rect(PANEL_X, 636, PANEL_W, 44), L"Main menu"});
    }
    return v;
}

// ---- game flow ----

static bool aiSide(bool white) { return white ? app.aiWhite : app.aiBlack; }

static void startAI() {
    app.thinking = true;
    Position p = app.hist.back();
    int depth = DEPTHS[app.level], id = app.gameId;
    HWND h = app.hwnd;
    std::thread([p, depth, id, h]() {
        Move m = bestMove(p, depth);
        PostMessageW(h, WM_AI_DONE, (WPARAM)id, (LPARAM)(m.from | (m.to << 8) | ((int)m.promo << 16)));
    }).detach();
}

static void refresh() {
    const Position& p = app.hist.back();
    app.legal = legalMoves(p);
    app.check = inCheck(p, p.white);
    app.over = false;
    app.result.clear();
    if (app.legal.empty()) {
        app.over = true;
        app.result = app.check ? (p.white ? L"Checkmate - Black wins" : L"Checkmate - White wins") : L"Stalemate - Draw";
    } else if (p.half >= 100) {
        app.over = true;
        app.result = L"Draw - fifty-move rule";
    } else if (insufficientMaterial(p)) {
        app.over = true;
        app.result = L"Draw - insufficient material";
    }
    if (!app.over && aiSide(p.white)) startAI();
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

static void newGame(int mode) {
    app.gameId++;
    app.mode = mode;
    app.aiWhite = (mode == 2);
    app.aiBlack = (mode == 1);
    app.flipped = (mode == 2);
    app.hist.assign(1, initial());
    app.played.clear();
    app.sel = -1;
    app.dragging = false;
    app.promoPending = false;
    app.thinking = false;
    app.screen = SCR_GAME;
    refresh();
}

static void doMove(const Move& m) {
    app.hist.push_back(apply(app.hist.back(), m));
    app.played.push_back(m);
    app.sel = -1;
    app.dragging = false;
    app.promoPending = false;
    refresh();
}

static bool humanTurn() { return !app.over && !app.thinking && !aiSide(app.hist.back().white); }

static bool isTarget(int from, int to) {
    for (const Move& m : app.legal)
        if (m.from == from && m.to == to) return true;
    return false;
}

static bool tryMove(int from, int to) {
    for (const Move& m : app.legal) {
        if (m.from != from || m.to != to) continue;
        if (m.promo) {
            app.promoPending = true;
            app.pFrom = from;
            app.pTo = to;
            InvalidateRect(app.hwnd, nullptr, FALSE);
        } else {
            doMove(m);
        }
        return true;
    }
    return false;
}

static void undo() {
    if (app.thinking || app.hist.size() <= 1) return;
    size_t n = app.hist.size();
    bool found = false;
    while (n > 1) {
        n--;
        if (!aiSide(app.hist[n - 1].white)) { found = true; break; }
    }
    if (!found) return;
    app.hist.resize(n);
    app.played.resize(n - 1);
    app.sel = -1;
    app.promoPending = false;
    refresh();
}

static void onButton(int id) {
    switch (id) {
        case B_WHITE: newGame(1); break;
        case B_BLACK: newGame(2); break;
        case B_TWO: newGame(3); break;
        case B_LEVEL: app.level = (app.level + 1) % 3; break;
        case B_QUIT: PostQuitMessage(0); break;
        case B_UNDO: undo(); break;
        case B_FLIP: app.flipped = !app.flipped; break;
        case B_MENU:
            app.gameId++;
            app.thinking = false;
            app.dragging = false;
            app.screen = SCR_MENU;
            break;
    }
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

// ---- drawing ----

static int glyphIndex(char t) {
    switch (t) {
        case 'K': return 0;
        case 'Q': return 1;
        case 'R': return 2;
        case 'B': return 3;
        case 'N': return 4;
        default: return 5;
    }
}

// Solid chess glyphs U+265A..U+265F, centred horizontally and bottom-aligned so every piece sits on the same baseline.
static void buildGlyphs() {
    FontFamily ff(L"Segoe UI Symbol");
    StringFormat sf;
    for (int i = 0; i < 6; i++) {
        wchar_t ch = (wchar_t)(0x265A + i);
        auto path = std::make_unique<GraphicsPath>();
        path->AddString(&ch, 1, &ff, FontStyleRegular, SQ * 0.9f, PointF(0, 0), &sf);
        RectF b;
        path->GetBounds(&b);
        Matrix m;
        m.Translate(-(b.X + b.Width / 2), SQ * 0.36f - (b.Y + b.Height));
        path->Transform(&m);
        g_glyph[i] = std::move(path);
    }
}

static void drawPiece(Graphics& g, char c, float cx, float cy) {
    bool w = isWhitePiece(c);
    SolidBrush fill(w ? Color(250, 250, 240) : Color(30, 30, 34));
    Pen pen(w ? Color(25, 25, 25) : Color(215, 215, 215), w ? 1.8f : 1.2f);
    pen.SetLineJoin(LineJoinRound);
    GraphicsPath* path = g_glyph[glyphIndex(up(c))].get();
    g.TranslateTransform(cx, cy);
    g.FillPath(&fill, path);
    g.DrawPath(&pen, path);
    g.ResetTransform();
}

static void drawText(Graphics& g, const std::wstring& s, float x, float y, float w, float h, float size, Color col,
                     bool bold = false, bool center = false) {
    FontFamily ff(L"Segoe UI");
    Font font(&ff, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat sf;
    sf.SetAlignment(center ? StringAlignmentCenter : StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush br(col);
    g.DrawString(s.c_str(), -1, &font, RectF(x, y, w, h), &sf, &br);
}

static void roundedPath(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void fillRounded(Graphics& g, const Brush* b, float x, float y, float w, float h, float r) {
    GraphicsPath p;
    roundedPath(p, x, y, w, h, r);
    g.FillPath(b, &p);
}

static void drawButtons(Graphics& g) {
    for (const Button& b : buttons()) {
        bool hot = b.rc.Contains(app.mouse.x, app.mouse.y);
        SolidBrush br(hot ? Color(92, 138, 84) : Color(62, 60, 57));
        fillRounded(g, &br, (float)b.rc.X, (float)b.rc.Y, (float)b.rc.Width, (float)b.rc.Height, 8);
        drawText(g, b.text, (float)b.rc.X, (float)b.rc.Y, (float)b.rc.Width, (float)b.rc.Height, 20, Color(240, 240, 235),
                 false, true);
    }
}

static void paintBackground(Graphics& g) {
    LinearGradientBrush bg(Rect(0, 0, CW, CH), Color(46, 43, 40), Color(24, 23, 22), LinearGradientModeVertical);
    g.FillRectangle(&bg, 0, 0, CW, CH);
}

static void paintMenu(Graphics& g) {
    paintBackground(g);
    drawText(g, L"CHESS", 0, 25, (float)CW, 110, 88, Color(240, 240, 235), true, true);
    SolidBrush light(Color(240, 217, 181)), dark(Color(181, 136, 99));
    const int x0 = (CW - BOARD) / 2, y0 = 150;
    for (int row = 0; row < 2; row++)
        for (int i = 0; i < 8; i++) {
            g.FillRectangle(((row + i) % 2) ? &dark : &light, x0 + i * SQ, y0 + row * SQ, SQ, SQ);
            const char* back = "rnbqkbnr";
            char c = row == 0 ? back[i] : (char)std::toupper((unsigned char)back[i]);
            drawPiece(g, c, (float)(x0 + i * SQ + SQ / 2), (float)(y0 + row * SQ + SQ / 2));
        }
    drawButtons(g);
}

static void paintGame(Graphics& g) {
    paintBackground(g);
    const Position& p = app.hist.back();
    int lastFrom = -1, lastTo = -1;
    if (!app.played.empty()) {
        lastFrom = app.played.back().from;
        lastTo = app.played.back().to;
    }
    int kingSq = -1;
    if (app.check)
        for (int s = 0; s < 64; s++)
            if (p.b[s] == (p.white ? 'K' : 'k')) kingSq = s;

    SolidBrush light(Color(240, 217, 181)), dark(Color(181, 136, 99));
    SolidBrush lastB(Color(120, 205, 210, 60)), selB(Color(130, 246, 246, 80)), checkB(Color(170, 220, 40, 40));
    for (int s = 0; s < 64; s++) {
        int x, y;
        squareOrigin(s, x, y);
        g.FillRectangle(((F(s) + R(s)) % 2 == 0) ? &dark : &light, x, y, SQ, SQ);
        if (s == lastFrom || s == lastTo) g.FillRectangle(&lastB, x, y, SQ, SQ);
        if (s == app.sel) g.FillRectangle(&selB, x, y, SQ, SQ);
        if (s == kingSq) g.FillRectangle(&checkB, x, y, SQ, SQ);
    }

    for (int i = 0; i < 8; i++) {
        std::wstring file(1, (wchar_t)(L'a' + (app.flipped ? 7 - i : i)));
        std::wstring rank(1, (wchar_t)(L'1' + (app.flipped ? i : 7 - i)));
        drawText(g, file, (float)(BX + i * SQ), (float)(BY + BOARD + 2), (float)SQ, 28, 18, Color(190, 190, 185), false, true);
        drawText(g, rank, 6, (float)(BY + i * SQ), 28, (float)SQ, 18, Color(190, 190, 185), false, true);
    }

    if (app.sel >= 0) {
        SolidBrush dot(Color(90, 20, 20, 20));
        Pen ring(Color(90, 20, 20, 20), 6);
        for (const Move& m : app.legal) {
            if (m.from != app.sel) continue;
            int x, y;
            squareOrigin(m.to, x, y);
            if (p.b[m.to] == '.') g.FillEllipse(&dot, x + SQ / 2 - 12, y + SQ / 2 - 12, 24, 24);
            else g.DrawEllipse(&ring, x + 5, y + 5, SQ - 10, SQ - 10);
        }
    }

    for (int s = 0; s < 64; s++) {
        if (p.b[s] == '.' || (app.dragging && s == app.sel)) continue;
        int x, y;
        squareOrigin(s, x, y);
        drawPiece(g, p.b[s], (float)(x + SQ / 2), (float)(y + SQ / 2));
    }

    if (app.dragging && app.sel >= 0) {
        int hover = squareAt(app.mouse.x, app.mouse.y);
        if (hover >= 0) {
            int x, y;
            squareOrigin(hover, x, y);
            Pen hp(Color(230, 255, 255, 255), 4);
            g.DrawRectangle(&hp, x + 2, y + 2, SQ - 4, SQ - 4);
        }
        drawPiece(g, p.b[app.sel], (float)app.mouse.x, (float)app.mouse.y);
    }

    // side panel
    drawText(g, L"Chess", PANEL_X, 30, PANEL_W, 50, 38, Color(240, 240, 235), true);
    std::wstring who = app.mode == 3 ? L"Two players"
                                     : std::wstring(app.mode == 1 ? L"You: White" : L"You: Black") + L"  |  " + LEVELS[app.level];
    drawText(g, who, PANEL_X, 82, PANEL_W, 26, 16, Color(170, 170, 165));

    std::wstring status;
    Color statusCol(240, 240, 235);
    if (app.over) {
        status = app.result;
        statusCol = Color(255, 205, 90);
    } else if (app.thinking) {
        status = L"Computer is thinking...";
    } else {
        status = std::wstring(p.white ? L"White" : L"Black") + L" to move";
        if (app.check) {
            status += L" - check!";
            statusCol = Color(255, 120, 110);
        }
    }
    SolidBrush box(Color(60, 58, 55));
    fillRounded(g, &box, PANEL_X, 120, PANEL_W, 50, 8);
    drawText(g, status, PANEL_X + 10, 120, PANEL_W - 20, 50, 18, statusCol, true);

    drawText(g, L"Moves", PANEL_X, 185, PANEL_W, 28, 18, Color(170, 170, 165), true);
    int rows = (int)(app.played.size() + 1) / 2;
    int start = std::max(0, rows - 13);
    for (int r = start; r < rows; r++) {
        float y = 215.f + (r - start) * 23;
        drawText(g, std::to_wstring(r + 1) + L".", PANEL_X, y, 40, 23, 16, Color(140, 140, 135));
        drawText(g, widen(moveName(app.played[r * 2])), PANEL_X + 40, y, 90, 23, 16, Color(225, 225, 220));
        if ((size_t)(r * 2 + 1) < app.played.size())
            drawText(g, widen(moveName(app.played[r * 2 + 1])), PANEL_X + 135, y, 90, 23, 16, Color(225, 225, 220));
    }
    drawButtons(g);

    if (app.over) {
        SolidBrush bn(Color(225, 20, 20, 20));
        fillRounded(g, &bn, BX + 60, BY + BOARD / 2 - 55, BOARD - 120, 110, 14);
        drawText(g, app.result, BX + 60, BY + BOARD / 2 - 55, BOARD - 120, 110, 36, Color(255, 205, 90), true, true);
    }

    if (app.promoPending) {
        SolidBrush dim(Color(150, 0, 0, 0)), panel(Color(235, 40, 38, 36));
        g.FillRectangle(&dim, BX, BY, BOARD, BOARD);
        fillRounded(g, &panel, (float)(BX + (BOARD - 390) / 2 - 15), (float)(BY + (BOARD - 90) / 2 - 15), 420, 120, 12);
        const char* order = "QRBN";
        for (int i = 0; i < 4; i++) {
            Rect rc = promoRect(i);
            bool hot = rc.Contains(app.mouse.x, app.mouse.y);
            SolidBrush cell(hot ? Color(250, 235, 200) : Color(240, 217, 181));
            fillRounded(g, &cell, (float)rc.X, (float)rc.Y, (float)rc.Width, (float)rc.Height, 8);
            char c = p.white ? order[i] : (char)std::tolower((unsigned char)order[i]);
            drawPiece(g, c, rc.X + rc.Width / 2.f, rc.Y + rc.Height / 2.f);
        }
    }
}

static void paint(HDC hdc) {
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, CW, CH);
    HGDIOBJ old = SelectObject(mem, bmp);
    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        if (app.screen == SCR_MENU) paintMenu(g);
        else paintGame(g);
    }
    BitBlt(hdc, 0, 0, CW, CH, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ---- window ----

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE:
            app.mouse = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            app.mouse = {x, y};
            for (const Button& b : buttons())
                if (b.rc.Contains(x, y)) {
                    onButton(b.id);
                    return 0;
                }
            if (app.screen != SCR_GAME) return 0;
            if (app.promoPending) {
                for (int i = 0; i < 4; i++) {
                    if (!promoRect(i).Contains(x, y)) continue;
                    char promo = "QRBN"[i];
                    for (const Move& m : app.legal)
                        if (m.from == app.pFrom && m.to == app.pTo && m.promo == promo) {
                            doMove(m);
                            return 0;
                        }
                }
                app.promoPending = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (!humanTurn()) return 0;
            int s = squareAt(x, y);
            if (s < 0) {
                app.sel = -1;
            } else if (app.sel >= 0 && s != app.sel && isTarget(app.sel, s)) {
                tryMove(app.sel, s);
                return 0;
            } else {
                char c = app.hist.back().b[s];
                if (c != '.' && isWhitePiece(c) == app.hist.back().white) {
                    app.sel = s;
                    app.dragging = true;
                    SetCapture(hwnd);
                } else {
                    app.sel = -1;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!app.dragging) return 0;
            app.dragging = false;
            ReleaseCapture();
            int s = squareAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (s >= 0 && s != app.sel) tryMove(app.sel, s);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_AI_DONE: {
            if ((int)wp != app.gameId || !app.thinking) return 0;
            app.thinking = false;
            Move m{(int)(lp & 0xFF), (int)((lp >> 8) & 0xFF), (char)((lp >> 16) & 0xFF)};
            doMove(m);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    GdiplusStartupInput gsi;
    ULONG_PTR token;
    GdiplusStartup(&token, &gsi, nullptr);
    buildGlyphs();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ChessWnd";
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc{0, 0, CW, CH};
    AdjustWindowRect(&rc, style, FALSE);
    app.hwnd = CreateWindowExW(0, wc.lpszClassName, L"Chess", style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                               rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    ShowWindow(app.hwnd, show);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    for (auto& p : g_glyph) p.reset();
    GdiplusShutdown(token);
    return 0;
}
