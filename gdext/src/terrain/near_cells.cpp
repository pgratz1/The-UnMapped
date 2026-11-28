#include "terrain/near_cells.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace unmapped::terrain {

NearGrid build_near_grid(const Layout& layout, std::span<const float> heights) {
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    if (heights.size() != total) {
        throw std::invalid_argument("build_near_grid: heights has " + std::to_string(heights.size()) +
                                    " cells, expected " + std::to_string(total));
    }
    NearGrid grid;
    grid.m = n / NEAR_CELL_CELLS;
    grid.boxes.resize(static_cast<std::size_t>(grid.m) * static_cast<std::size_t>(grid.m));
    for (int j = 0; j < grid.m; ++j) {
        for (int i = 0; i < grid.m; ++i) {
            NearBox& b = grid.boxes[static_cast<std::size_t>(j) * static_cast<std::size_t>(grid.m) + static_cast<std::size_t>(i)];
            b.x0 = 16.0 * static_cast<double>(16 * i - n / 2) - 8.0;
            b.x1 = b.x0 + NEAR_CELL_M;
            b.z0 = 16.0 * static_cast<double>(16 * j - n / 2) - 8.0;
            b.z1 = b.z0 + NEAR_CELL_M;
            float lo = heights[0];
            float hi = heights[0];
            bool first = true;
            for (int y = std::max(0, 16 * j - 1); y <= std::min(n - 1, 16 * j + 16); ++y) {
                for (int x = std::max(0, 16 * i - 1); x <= std::min(n - 1, 16 * i + 16); ++x) {
                    const float h = heights[static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x)];
                    if (first) {
                        lo = h;
                        hi = h;
                        first = false;
                    } else {
                        lo = std::min(lo, h);
                        hi = std::max(hi, h);
                    }
                }
            }
            b.bot = static_cast<double>(lo) - NEAR_BOX_BELOW;
            b.top = static_cast<double>(hi) + NEAR_BOX_ABOVE;
        }
    }
    return grid;
}

namespace {

double gap(double lo, double hi, double p) noexcept { return std::max(0.0, std::max(lo - p, p - hi)); }

}  // namespace

double near_distance(const NearBox& box, const NearViewer& v) noexcept {
    const double dx = gap(box.x0, box.x1, v.x);
    const double dz = gap(box.z0, box.z1, v.z);
    const double dy = gap(box.bot, box.top, v.y);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double near_distance_h(const NearBox& box, const NearViewer& v) noexcept {
    const double dx = gap(box.x0, box.x1, v.x);
    const double dz = gap(box.z0, box.z1, v.z);
    return std::sqrt(dx * dx + dz * dz);
}

bool near_in_view(const NearBox& box, const NearViewer& v) noexcept {
    if (near_distance_h(box, v) == 0.0) {
        return true;
    }
    const double h = v.heading_deg * 3.14159265358979323846 / 180.0;
    const double fx = std::sin(h);
    const double fz = -std::cos(h);
    const double ux = 0.5 * (box.x0 + box.x1) - v.x;
    const double uz = 0.5 * (box.z0 + box.z1) - v.z;
    const double len = std::sqrt(ux * ux + uz * uz);
    if (len == 0.0) {
        return true;
    }
    return (fx * ux + fz * uz) / len >= 0.5;
}

NearCells::NearCells(NearGrid grid, NearParams params)
    : grid_(std::move(grid)), params_(params), cells_(grid_.boxes.size()) {}

void NearCells::set_state(int q, NearState s) {
    Cell& c = cells_[static_cast<std::size_t>(q)];
    if (c.state == NearState::computing) {
        --computing_;
    }
    if (s == NearState::computing) {
        ++computing_;
    }
    c.state = s;
    if (s == NearState::empty) {
        active_.erase(q);
        c.abandoned = false;
        c.late_counted = false;
        c.n = 0;
    } else {
        active_.insert(q);
    }
}

bool NearCells::before(int a, int b) const {
    const Cell& ca = cells_[static_cast<std::size_t>(a)];
    const Cell& cb = cells_[static_cast<std::size_t>(b)];
    const int va = ca.in_view ? 0 : 1;
    const int vb = cb.in_view ? 0 : 1;
    if (va != vb) {
        return va < vb;
    }
    if (ca.d != cb.d) {
        return ca.d < cb.d;
    }
    return a < b;
}

void NearCells::update(const NearViewer& viewer) {
    const double R = params_.radius;
    const double RH = params_.radius + params_.hysteresis;
    const int m = grid_.m;
    // The square of half-side R + H + 256 around the viewer, in cells.
    const double half = RH + NEAR_CELL_M;
    const double base = grid_.boxes.empty() ? 0.0 : grid_.boxes[0].x0;
    auto index = [base](double p) { return static_cast<int>(std::floor((p - base) / NEAR_CELL_M)); };
    const int i0 = std::max(0, index(viewer.x - half));
    const int i1 = std::min(m - 1, index(viewer.x + half));
    const int j0 = std::max(0, index(viewer.z - half));
    const int j1 = std::min(m - 1, index(viewer.z + half));
    std::set<int> visit = active_;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            visit.insert(j * m + i);
        }
    }
    wanted_.clear();
    for (const int q : visit) {
        Cell& c = cells_[static_cast<std::size_t>(q)];
        const NearBox& box = grid_.boxes[static_cast<std::size_t>(q)];
        c.d = near_distance(box, viewer);
        c.in_view = near_in_view(box, viewer);
        const double d = c.d;
        if (d <= R) {
            wanted_.push_back(q);
        }
        switch (c.state) {
            case NearState::empty:
                if (d <= R) {
                    set_state(q, NearState::queued);
                }
                break;
            case NearState::queued:
                if (d > RH) {
                    set_state(q, NearState::empty);
                }
                break;
            case NearState::computing:
                if (d <= R) {
                    c.abandoned = false;
                } else if (d > RH) {
                    c.abandoned = true;
                }
                break;
            case NearState::ready:
                if (d > RH) {
                    set_state(q, NearState::empty);
                    released_.push_back(q);
                }
                break;
            case NearState::filled:
                if (d > RH) {
                    set_state(q, NearState::dropping);
                }
                break;
            case NearState::dropping:
                if (d <= RH) {
                    set_state(q, NearState::filled);
                }
                break;
        }
        const NearState s = c.state;
        if ((s == NearState::queued || s == NearState::computing || s == NearState::ready) && !c.abandoned &&
            !c.late_counted && d <= NEAR_VISIBLE_MAX) {
            c.late_counted = true;
            ++late_;
        }
    }
}

std::vector<int> NearCells::start_computing() {
    std::vector<int> queued;
    for (const int q : active_) {
        if (cells_[static_cast<std::size_t>(q)].state == NearState::queued) {
            queued.push_back(q);
        }
    }
    std::sort(queued.begin(), queued.end(), [this](int a, int b) { return before(a, b); });
    const std::uint64_t limit = static_cast<std::uint64_t>(std::max(0, params_.max_in_flight));
    std::vector<int> out;
    for (const int q : queued) {
        if (computing_ >= limit) {
            break;
        }
        set_state(q, NearState::computing);
        cells_[static_cast<std::size_t>(q)].abandoned = false;
        out.push_back(q);
    }
    return out;
}

void NearCells::computed(int q, std::uint64_t n) {
    Cell& c = cells_.at(static_cast<std::size_t>(q));
    if (c.state != NearState::computing) {
        return;
    }
    if (c.abandoned) {
        set_state(q, NearState::empty);
        released_.push_back(q);
        return;
    }
    set_state(q, NearState::ready);
    c.n = n;
}

std::vector<int> NearCells::take_attach() {
    std::vector<int> ready;
    for (const int q : active_) {
        if (cells_[static_cast<std::size_t>(q)].state == NearState::ready) {
            ready.push_back(q);
        }
    }
    std::sort(ready.begin(), ready.end(), [this](int a, int b) { return before(a, b); });
    std::vector<int> out;
    std::uint64_t instances = 0;
    for (const int q : ready) {
        const std::uint64_t n = cells_[static_cast<std::size_t>(q)].n;
        if (!out.empty() && (static_cast<int>(out.size()) >= params_.max_attach_cells ||
                             instances + n > params_.max_attach_instances)) {
            break;
        }
        out.push_back(q);
        instances += n;
    }
    for (const int q : out) {
        set_state(q, NearState::filled);
    }
    return out;
}

std::vector<int> NearCells::take_drops() {
    std::vector<int> dropping;
    for (const int q : active_) {
        if (cells_[static_cast<std::size_t>(q)].state == NearState::dropping) {
            dropping.push_back(q);
        }
    }
    std::sort(dropping.begin(), dropping.end(), [this](int a, int b) {
        const double da = cells_[static_cast<std::size_t>(a)].d;
        const double db = cells_[static_cast<std::size_t>(b)].d;
        return da != db ? da > db : a < b;
    });
    if (static_cast<int>(dropping.size()) > params_.max_drop_cells) {
        dropping.resize(static_cast<std::size_t>(std::max(0, params_.max_drop_cells)));
    }
    for (const int q : dropping) {
        set_state(q, NearState::empty);
    }
    return dropping;
}

std::vector<int> NearCells::take_released() {
    std::vector<int> out;
    out.swap(released_);
    return out;
}

NearCounts NearCells::counts() const noexcept {
    NearCounts k;
    for (const int q : active_) {
        const Cell& c = cells_[static_cast<std::size_t>(q)];
        switch (c.state) {
            case NearState::filled:
            case NearState::dropping:
                ++k.filled;
                k.instances += c.n;
                break;
            case NearState::queued:
                ++k.queued;
                break;
            case NearState::computing:
                ++k.computing;
                break;
            case NearState::ready:
                ++k.ready;
                break;
            case NearState::empty:
                break;
        }
    }
    k.late = late_;
    return k;
}

bool NearCells::settled() const {
    for (const int q : active_) {
        const NearState s = cells_[static_cast<std::size_t>(q)].state;
        if (s != NearState::filled) {
            return false;
        }
    }
    for (const int q : wanted_) {
        if (cells_[static_cast<std::size_t>(q)].state != NearState::filled) {
            return false;
        }
    }
    return true;
}

int forest_column(const Layout& layout, const GroundCells& cells) {
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    if (cells.type.size() != total) {
        return n / 2;
    }
    int best = n / 8;
    int best_count = -1;
    for (int x = n / 8; x < 7 * n / 8; ++x) {
        int count = 0;
        for (int y = n / 8; y < 7 * n / 8; ++y) {
            const std::uint8_t t = cells.type[static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x)];
            count += (t == 2 || t == 3) ? 1 : 0;
        }
        if (count > best_count) {
            best_count = count;
            best = x;
        }
    }
    return best;
}

}  // namespace unmapped::terrain
