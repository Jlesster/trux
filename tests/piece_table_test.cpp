#include "test.hpp"

#include <trux/text/piece_table.hpp>

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <string>
#include <vector>

using trux::text::PieceTable;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if(!(cond)) {                                                        \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__,      \
                         __LINE__, #cond);                                   \
            std::abort();                                                    \
        }                                                                    \
    } while(0)

namespace {

/// Reference implementation: same grouping rules, plain std::string.
struct Model {
    std::string              text;
    std::deque<std::string>  undo;
    std::vector<std::string> redo;
    bool                     open{false};

    void begin() {
        redo.clear();
        if(open) return;
        undo.push_back(text);
        open = true;
    }
    void insert(size_t pos, const std::string& s) {
        if(s.empty()) return;
        pos = std::min(pos, text.size());
        begin();
        text.insert(pos, s);
    }
    void erase(size_t pos, size_t len) {
        if(pos >= text.size() || len == 0) return;
        len = std::min(len, text.size() - pos);
        begin();
        text.erase(pos, len);
    }
    void seal() { open = false; }
    bool do_undo() {
        open = false;
        if(undo.empty()) return false;
        redo.push_back(text);
        text = undo.back();
        undo.pop_back();
        return true;
    }
    bool do_redo() {
        open = false;
        if(redo.empty()) return false;
        undo.push_back(text);
        text = redo.back();
        redo.pop_back();
        return true;
    }

    size_t line_count() const {
        return 1 + static_cast<size_t>(std::count(text.begin(), text.end(), '\n'));
    }
    size_t line_start(size_t n) const {
        n = std::min(n, line_count() - 1);
        size_t pos = 0;
        for(size_t i = 0; i < n; i++) pos = text.find('\n', pos) + 1;
        return pos;
    }
    size_t line_end(size_t n) const {
        size_t e = text.find('\n', line_start(n));
        return e == std::string::npos ? text.size() : e;
    }
    size_t line_of(size_t pos) const {
        pos = std::min(pos, text.size());
        return static_cast<size_t>(
            std::count(text.begin(), text.begin() + static_cast<long>(pos), '\n'));
    }
    size_t prev_char(size_t pos) const {
        pos = std::min(pos, text.size());
        if(pos == 0) return 0;
        size_t i = pos - 1;
        while(i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) i--;
        return i;
    }
    size_t next_char(size_t pos) const {
        if(pos >= text.size()) return text.size();
        size_t i = pos + 1;
        while(i < text.size() &&
              (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
            i++;
        return i;
    }
};

void check_same(const PieceTable& t, const Model& m, std::mt19937& rng) {
    CHECK(t.size() == m.text.size());
    CHECK(t.empty() == m.text.empty());
    CHECK(t.str() == m.text);
    CHECK(t.line_count() == m.line_count());

    auto pick = [&](size_t n) { return n ? rng() % n : 0; };

    for(int k = 0; k < 3; k++) {
        size_t line = pick(m.line_count() + 1);
        CHECK(t.line_start(line) == m.line_start(line));
        CHECK(t.line_end(line) == m.line_end(line));
        CHECK(t.line_text(line) ==
              m.text.substr(m.line_start(line),
                            m.line_end(line) - m.line_start(line)));

        size_t pos = pick(m.text.size() + 2);
        CHECK(t.line_of(pos) == m.line_of(pos));
        CHECK(t.prev_char(pos) == m.prev_char(pos));
        CHECK(t.next_char(pos) == m.next_char(pos));
        CHECK(t.at(pos) == (pos < m.text.size() ? m.text[pos] : '\0'));

        size_t len = pick(m.text.size() + 2);
        CHECK(t.substr(pos, len) ==
              (pos >= m.text.size() ? std::string{} : m.text.substr(pos, len)));
    }
    CHECK(t.can_undo() == (m.open || !m.undo.empty()));
    CHECK(t.can_redo() == !m.redo.empty());
}

void fuzz(uint32_t seed, int steps, std::string initial) {
    std::mt19937 rng(seed);
    PieceTable   t(initial);
    Model        m;
    m.text = initial;

    static const char* chunks[] = {"a", "bc", "hello", "\n", "\n\n", "é",
                                   "日本", "😀", " ", "x\ny\nz"};
    size_t last_end = 0;

    for(int i = 0; i < steps; i++) {
        size_t sz = m.text.size();
        switch(rng() % 8) {
            case 0:
            case 1: {  // insert somewhere
                size_t pos = rng() % (sz + 3);
                std::string s = chunks[rng() % std::size(chunks)];
                t.insert(pos, s);
                m.insert(pos, s);
                last_end = std::min(pos, sz) + s.size();
                break;
            }
            case 2:
            case 3: {  // "typing": insert where the last insert ended
                std::string s = chunks[rng() % 5];
                t.insert(last_end, s);
                m.insert(last_end, s);
                last_end = std::min(last_end, sz) + s.size();
                break;
            }
            case 4: {
                size_t pos = rng() % (sz + 2), len = rng() % 6;
                t.erase(pos, len);
                m.erase(pos, len);
                break;
            }
            case 5: {  // replace
                size_t pos = rng() % (sz + 2), len = rng() % 4;
                std::string s = chunks[rng() % std::size(chunks)];
                t.replace(pos, len, s);
                m.erase(pos, len);
                m.insert(pos, s);
                break;
            }
            case 6: t.seal(); m.seal(); break;
            case 7: {
                bool u = rng() % 2;
                CHECK((u ? t.undo() : t.redo()) ==
                      (u ? m.do_undo() : m.do_redo()));
                break;
            }
        }
        check_same(t, m, rng);
    }

    // Unwind the whole history, then replay it.
    while(m.do_undo()) CHECK(t.undo());
    CHECK(!t.undo());
    CHECK(t.str() == m.text);
    while(m.do_redo()) CHECK(t.redo());
    CHECK(!t.redo());
    CHECK(t.str() == m.text);
}

}  // namespace

void test_empty_and_basic() {
    PieceTable t;
    CHECK(t.size() == 0 && t.line_count() == 1 && t.line_end(0) == 0);
    t.insert(0, "hello");
    t.insert(5, " world");
    t.insert(5, ",");
    CHECK(t.str() == "hello, world");
    t.erase(5, 1);
    CHECK(t.str() == "hello world");
    CHECK(t.at(100) == '\0');
}

void test_lines() {
    PieceTable t("one\ntwo\n\nfour");
    CHECK(t.line_count() == 4);
    CHECK(t.line_text(0) == "one" && t.line_text(2).empty() &&
          t.line_text(3) == "four");
    CHECK(t.line_of(3) == 0 && t.line_of(4) == 1 && t.line_of(100) == 3);
    t.insert(t.size(), "\n");
    CHECK(t.line_count() == 5 && t.line_text(4).empty());
}

void test_undo_grouping() {
    PieceTable t("ab");
    t.insert(2, "c");
    t.insert(3, "d");  // same group as 'c'
    t.seal();
    t.insert(4, "e");
    CHECK(t.str() == "abcde");
    CHECK(t.undo() && t.str() == "abcd");
    CHECK(t.undo() && t.str() == "ab");
    CHECK(!t.undo());
    CHECK(t.redo() && t.str() == "abcd");
    t.insert(0, "X");  // edit after undo drops redo history
    CHECK(!t.can_redo());
}

void test_utf8_steps() {
    PieceTable t("a日😀");
    CHECK(t.next_char(0) == 1 && t.next_char(1) == 4 && t.next_char(4) == 8);
    CHECK(t.prev_char(8) == 4 && t.prev_char(4) == 1 && t.prev_char(1) == 0);
}

void test_copy_is_independent() {
    PieceTable a("abc");
    a.insert(3, "d");
    a.seal();
    PieceTable b = a;
    b.insert(0, "X");
    a.erase(0, 1);
    CHECK(a.str() == "bcd" && b.str() == "Xabcd");
    CHECK(a.undo() && a.str() == "abcd");
    CHECK(b.undo() && b.str() == "abcd");
    CHECK(a.undo() && a.str() == "abc");
}

void test_fuzz() {
    for(uint32_t seed = 1; seed <= 40; seed++) fuzz(seed, 400, "");
    for(uint32_t seed = 100; seed <= 120; seed++)
        fuzz(seed, 400, "start\nmiddle é\n日本\nend");
}

int main() {
    test::run("empty_and_basic", test_empty_and_basic);
    test::run("lines", test_lines);
    test::run("undo_grouping", test_undo_grouping);
    test::run("utf8_steps", test_utf8_steps);
    test::run("copy_is_independent", test_copy_is_independent);
    test::run("fuzz_vs_string_model", test_fuzz);
    return test::summary();
}
