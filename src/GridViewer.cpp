#include <algorithm>
#include <iostream>
#include <limits>

#include "GridViewer.h"

const unsigned GridViewer::_cell_size = 30;
const unsigned GridViewer::_grid_width = 2;
const unsigned GridViewer::_iter_step = 1;
const float GridViewer::_zoom_step = 0.1f;
const float GridViewer::_min_zoom = 1.0f / _cell_size;    // Equal to one pixel
                                                          // per cell
const Color GridViewer::_grid_color{127, 127, 127, 255};
const Color GridViewer::_alive_color{0, 0, 0, 255};
const Color GridViewer::_dead_color{255, 255, 255, 255};

GridViewer::GridViewer()
    : _win("Title",
           Vector2D<int>(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED),
           Vector2D<int>(640, 480), 0),
      _llca("B3/S23"),
      _top_left(0, 0),
      _running(false),
      _iter_per_sec(1),
      _zoom_factor(1.0f),
      _max_zoom(get_max_zoom()) {
    // Glider
    _llca.set_cell_state(Vector2D<LLCA::CellPos>(0, 2), LLCA::CellState::ALIVE);
    _llca.set_cell_state(Vector2D<LLCA::CellPos>(1, 3), LLCA::CellState::ALIVE);
    _llca.set_cell_state(Vector2D<LLCA::CellPos>(2, 1), LLCA::CellState::ALIVE);
    _llca.set_cell_state(Vector2D<LLCA::CellPos>(2, 2), LLCA::CellState::ALIVE);
    _llca.set_cell_state(Vector2D<LLCA::CellPos>(2, 3), LLCA::CellState::ALIVE);
}

float GridViewer::get_max_zoom() const {
    const auto win_size = _win.get_size();
    const auto min_dimension = std::min(win_size.x, win_size.y);
    // Equal to 5x5 cells on screen
    return min_dimension / (5 * (_cell_size + _grid_width));
}

unsigned GridViewer::get_cell_size() const {
    return _cell_size * _zoom_factor;
}

unsigned GridViewer::get_grid_width() const {
    return _grid_width * _zoom_factor;
}

Vector2D<LLCA::CellPos> GridViewer::get_view_size() const {
    auto vec = _win.get_size() / (float) (get_cell_size() + get_grid_width());
    return Vector2D<LLCA::CellPos>(std::ceil(vec.x), std::ceil(vec.y));
}

// TODO: Use state machine
void GridViewer::loop() {
    Uint32 last_tick = 0;
    Uint32 next_tick = 0;
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                return;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    _running = !_running;
                    last_tick = SDL_GetTicks();
                    next_tick = last_tick + 1000 / _iter_per_sec;
                } else if (event.key.keysym.sym == SDLK_MINUS
                           && _iter_per_sec > _iter_step) {
                    _iter_per_sec -= _iter_step;
                } else if (event.key.keysym.sym == SDLK_PLUS
                           && _iter_per_sec <= std::numeric_limits<Uint32>::max()
                                                   - _iter_step) {
                    _iter_per_sec += _iter_step;
                } else {
                    Vector2D<LLCA::CellPos> move_by(
                        (event.key.keysym.sym == SDLK_l)
                            - (event.key.keysym.sym == SDLK_h),
                        (event.key.keysym.sym == SDLK_j)
                            - (event.key.keysym.sym == SDLK_k));
                    _top_left += move_by;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN
                       && event.button.button == SDL_BUTTON_LEFT) {
                Vector2D<int32_t> click_pos(event.button.x, event.button.y);
                auto cell_pos = screen_to_cell_pos(click_pos);
                _drawing = true;
                _llca.toggle_cell_state(cell_pos);
                _draw_state = _llca.get_cell_state(cell_pos);
            } else if (event.type == SDL_MOUSEBUTTONUP
                       && event.button.button == SDL_BUTTON_LEFT) {
                _drawing = false;
                // Reset draw cycle
                last_tick = SDL_GetTicks();
                next_tick = last_tick + 1000 / _iter_per_sec;
            } else if (event.type == SDL_MOUSEMOTION && _drawing) {
                Vector2D<int32_t> mouse_pos(event.motion.x, event.motion.y);
                auto cell_pos = screen_to_cell_pos(mouse_pos);
                _llca.set_cell_state(cell_pos, _draw_state);
            } else if (event.type == SDL_MOUSEWHEEL) {
                zoom(event.wheel.y * _zoom_step);
            }
        }

        if (_running && !_drawing && SDL_GetTicks() >= next_tick) {
            _llca.advance();
            Uint32 ticks_per_update = SDL_GetTicks() - last_tick;
            std::cout << ticks_per_update << std::endl;
            last_tick = SDL_GetTicks();
            next_tick += 1000 / _iter_per_sec;
        }
        draw();
    }
}

void GridViewer::zoom(float factor) {
    // The cursor should be on the same cell before and after zooming
    auto mouse_pos = _win.get_mouse_pos();
    auto old_cell_pos = screen_to_cell_pos(mouse_pos);
    _zoom_factor = std::clamp(_zoom_factor + factor, _min_zoom, _max_zoom);
    auto new_cell_pos = screen_to_cell_pos(mouse_pos);
    _top_left += old_cell_pos - new_cell_pos;
}

Vector2D<LLCA::CellPos>
    GridViewer::screen_to_cell_pos(Vector2D<int32_t> screen_pos) const {
    Vector2D<LLCA::CellPos> pos(screen_pos.x, screen_pos.y);
    return _top_left + pos / (get_cell_size() + get_grid_width());
}

void GridViewer::draw() {
    _win.set_draw_color(_grid_color);
    _win.clear();

    const auto view_size = get_view_size();
    const auto cell_size = get_cell_size();
    const auto grid_width = get_grid_width();
    const auto size_per_cell = cell_size + grid_width;
    const Vector2D<int> cell_rect_dimensions(cell_size, cell_size);

    for (LLCA::CellPos x = 0; x < view_size.x; x++) {
        for (LLCA::CellPos y = 0; y < view_size.y; y++) {
            auto cell_pos = _top_left + Vector2D<LLCA::CellPos>(x, y);
            if (_llca.get_cell_state(cell_pos) == LLCA::CellState::ALIVE) {
                _win.set_draw_color(_alive_color);
            } else {
                _win.set_draw_color(_dead_color);
            }

            const Vector2D<int> pos(x * size_per_cell + grid_width / 2,
                                    y * size_per_cell + grid_width / 2);
            _win.draw_rect(pos, cell_rect_dimensions);
        }
    }

    _win.update();
}
