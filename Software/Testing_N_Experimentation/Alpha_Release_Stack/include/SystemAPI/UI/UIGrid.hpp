/*****************************************************************
 * @file UIGrid.hpp
 * @brief UI Framework Grid - Grid layout container
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIContainer.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Grid layout container
 * 
 * @example
 * ```cpp
 * auto grid = new UIGrid(3, 3);  // 3x3 grid
 * grid->setGap(4);
 * 
 * // Add items
 * for (int i = 0; i < 9; i++) {
 *   grid->addChild(new UIButton(std::to_string(i + 1).c_str()));
 * }
 * 
 * // Or set specific cell
 * grid->setCell(0, 0, new UIIcon(IconType::HOME));
 * ```
 */
class UIGrid : public UIContainer {
public:
  UIGrid() {
    setLayoutMode(LayoutMode::NONE);  // We handle layout ourselves
  }
  
  UIGrid(int cols, int rows) : UIGrid() {
    setGridSize(cols, rows);
  }
  
  const char* getTypeName() const override { return "UIGrid"; }
  
  // ---- Grid Configuration ----
  
  void setGridSize(int cols, int rows) {
    cols_ = cols;
    rows_ = rows;
    cells_.resize(cols * rows, nullptr);
    markDirty();
  }
  
  int getColumns() const { return cols_; }
  int getRows() const { return rows_; }
  
  void setCellGap(uint8_t gap) { cellGap_ = gap; markDirty(); }
  uint8_t getCellGap() const { return cellGap_; }
  
  // ---- Cell Access ----
  
  void setCell(int col, int row, UIElement* element) {
    int index = row * cols_ + col;
    if (index >= 0 && index < (int)cells_.size()) {
      if (cells_[index]) {
        removeChild(cells_[index]);
      }
      cells_[index] = element;
      if (element) {
        addChild(element);
      }
      markDirty();
    }
  }
  
  UIElement* getCell(int col, int row) const {
    int index = row * cols_ + col;
    if (index >= 0 && index < (int)cells_.size()) {
      return cells_[index];
    }
    return nullptr;
  }
  
  void clearCell(int col, int row) {
    setCell(col, row, nullptr);
  }
  
  // ---- Cell Size ----
  
  void setCellSize(uint16_t width, uint16_t height) {
    fixedCellWidth_ = width;
    fixedCellHeight_ = height;
    markDirty();
  }
  
  Size getCellSize() const {
    Rect content = style_.contentRect(bounds_);
    uint16_t cellW = fixedCellWidth_ > 0 ? fixedCellWidth_ : 
                     (content.width - (cols_ - 1) * cellGap_) / cols_;
    uint16_t cellH = fixedCellHeight_ > 0 ? fixedCellHeight_ : 
                     (content.height - (rows_ - 1) * cellGap_) / rows_;
    return Size(cellW, cellH);
  }
  
  // ---- Selection ----
  
  void setSelectedCell(int col, int row) {
    if (col >= 0 && col < cols_ && row >= 0 && row < rows_) {
      if (auto* current = getCell(selectedCol_, selectedRow_)) {
        current->blur();
      }
      selectedCol_ = col;
      selectedRow_ = row;
      if (auto* next = getCell(col, row)) {
        next->focus();
      }
      markDirty();
    }
  }
  
  int getSelectedColumn() const { return selectedCol_; }
  int getSelectedRow() const { return selectedRow_; }
  UIElement* getSelectedCell() const { return getCell(selectedCol_, selectedRow_); }
  
  void selectUp() { setSelectedCell(selectedCol_, std::max(0, selectedRow_ - 1)); }
  void selectDown() { setSelectedCell(selectedCol_, std::min(rows_ - 1, selectedRow_ + 1)); }
  void selectLeft() { setSelectedCell(std::max(0, selectedCol_ - 1), selectedRow_); }
  void selectRight() { setSelectedCell(std::min(cols_ - 1, selectedCol_ + 1), selectedRow_); }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      switch (event.btn.button) {
        case Button::UP:
        case Button::ENCODER_CCW:
          selectUp();
          event.consumed = true;
          return true;
        case Button::DOWN:
        case Button::ENCODER_CW:
          selectDown();
          event.consumed = true;
          return true;
        case Button::LEFT:
          selectLeft();
          event.consumed = true;
          return true;
        case Button::RIGHT:
          selectRight();
          event.consumed = true;
          return true;
        default:
          break;
      }
    }
    
    // Forward to selected cell
    if (auto* cell = getSelectedCell()) {
      if (cell->handleInput(event)) {
        return true;
      }
    }
    
    return UIContainer::handleInput(event);
  }
  
  // ---- Layout ----
  
  void layout() override {
    Rect content = style_.contentRect(bounds_);
    Size cellSize = getCellSize();
    
    for (int row = 0; row < rows_; row++) {
      for (int col = 0; col < cols_; col++) {
        if (auto* cell = getCell(col, row)) {
          int16_t x = col * (cellSize.width + cellGap_);
          int16_t y = row * (cellSize.height + cellGap_);
          cell->setBounds(x, y, cellSize.width, cellSize.height);
          cell->layout();
        }
      }
    }
  }
  
  Size getPreferredSize() const override {
    Size cellSize = getCellSize();
    return Size(
      cols_ * cellSize.width + (cols_ - 1) * cellGap_ + style_.horizontalSpace(),
      rows_ * cellSize.height + (rows_ - 1) * cellGap_ + style_.verticalSpace()
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  int cols_ = 1;
  int rows_ = 1;
  uint8_t cellGap_ = 2;
  uint16_t fixedCellWidth_ = 0;
  uint16_t fixedCellHeight_ = 0;
  
  std::vector<UIElement*> cells_;
  
  int selectedCol_ = 0;
  int selectedRow_ = 0;
};

/**
 * @brief Tab container with header buttons
 */
class UITabs : public UIContainer {
public:
  UITabs() {
    setLayoutMode(LayoutMode::NONE);
  }
  
  const char* getTypeName() const override { return "UITabs"; }
  
  void addTab(const char* label, UIContainer* content) {
    tabs_.push_back({label, content});
    addChild(content);
    content->setVisible(tabs_.size() == 1);  // First tab visible
  }
  
  void setSelectedTab(int index) {
    if (index >= 0 && index < (int)tabs_.size()) {
      if (selectedTab_ >= 0 && selectedTab_ < (int)tabs_.size()) {
        tabs_[selectedTab_].content->setVisible(false);
      }
      selectedTab_ = index;
      tabs_[selectedTab_].content->setVisible(true);
      markDirty();
    }
  }
  
  int getSelectedTab() const { return selectedTab_; }
  int getTabCount() const { return tabs_.size(); }
  
  void nextTab() { setSelectedTab((selectedTab_ + 1) % tabs_.size()); }
  void prevTab() { setSelectedTab((selectedTab_ - 1 + tabs_.size()) % tabs_.size()); }
  
  void layout() override {
    Rect content = style_.contentRect(bounds_);
    int16_t tabBarHeight = 16;
    
    // Position tab content below header
    for (auto& tab : tabs_) {
      tab.content->setBounds(0, tabBarHeight, content.width, content.height - tabBarHeight);
      tab.content->layout();
    }
  }
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  struct Tab {
    const char* label;
    UIContainer* content;
  };
  
  std::vector<Tab> tabs_;
  int selectedTab_ = 0;
};

} // namespace UI
} // namespace SystemAPI
