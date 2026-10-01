#pragma once

#include "Theme.h"
#include "PatternView.h"
#include "PluginProcessor.h"

// The MIDI groove browser: library tree (Toontrack folder conventions), search and
// filters, the groove list and the preview / info panel.
class BrowserComponent : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer,
                         private juce::TableListBoxModel
{
public:
    explicit BrowserComponent (GroovePlayerProcessor&);
    ~BrowserComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    // Called ~30 times per second by the editor.
    void updatePlayState();

    std::function<void (int grooveIndex)> onAddToSong;

    int getSelectedGroove() const { return selectedGroove; }

private:
    class FolderItem;

    GroovePlayerProcessor& proc;
    AppContext& app;
    std::shared_ptr<const tt::LibraryIndex> index;

    juce::TreeView tree;
    std::unique_ptr<FolderItem> rootItem;

    juce::TextEditor searchBox;
    juce::ComboBox typeBox, feelBox, meterBox, tempoBox, styleBox;
    juce::TextButton favButton { "Favourites" }, scopeButton { "In folder" }, clearButton { "Clear" };
    juce::Label countLabel, statusLabel, folderLabel;

    juce::TableListBox table;
    std::vector<int> rows;
    bool searchMode = false, showLocation = false;
    int currentFolder = 0;

    PatternView pattern;
    juce::Label infoTitle, infoMeta;
    juce::TextButton playButton { "Play" }, addButton { "Add to song" };
    juce::ToggleButton autoPreviewToggle { "Auto-preview" }, origTempoToggle { "Original tempo" };

    int selectedGroove = -1;
    int previewingGroove = -1;
    bool suppressNextClick = false;
    bool wasScanning = false;

    enum Columns { colFav = 1, colName, colMeter, colTempo, colLength, colType, colLocation };

    void libraryChanged();
    void showFolder (int folder);
    void runSearch();
    void updateColumns();
    void selectGroove (int grooveIndex, bool preview);
    void togglePreview (int grooveIndex);
    void updateInfo (const tt::Groove*);
    void restoreSelectedFolder();
    tt::SearchFilter makeFilter() const;
    bool filtersActive() const;
    void resetFilters();
    juce::String lengthText (const tt::GrooveInfo&) const;
    void startDragOfGroove (int grooveIndex, juce::Component* source);
    void addRowToSong (int row);

    // TableListBoxModel
    int getNumRows() override { return (int) rows.size(); }
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void cellClicked (int row, int col, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int col, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRow) override;
    void sortOrderChanged (int col, bool forwards) override;
    void returnKeyPressed (int row) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
    juce::String getCellTooltip (int row, int col) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserComponent)
};
