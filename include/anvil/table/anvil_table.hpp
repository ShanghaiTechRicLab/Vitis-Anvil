#pragma once
#include <tabulate.hpp>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace anvil::table {

using Row = std::vector<std::string>;

inline void Print(std::initializer_list<Row> rows) {
    if (rows.size() == 0) return;
    tabulate::Table t;
    for (const auto& row : rows) {
        tabulate::Table::Row_t r;
        for (const auto& cell : row) r.push_back(cell);
        t.add_row(r);
    }
    if (t.size() > 0) {
        for (std::size_t i = 0; i < t[0].size(); ++i) {
            t[0][i].format().font_style({tabulate::FontStyle::bold});
        }
    }
    std::cout << t << "\n";
}

}  // namespace anvil::table
