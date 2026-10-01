#include "BrowserComponent.h"
#include "DragTypes.h"

namespace
{
    juce::Path starPath (juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
        return p;
    }

    enum TempoRange { tempoAny = 1, tempoNearHost, tempoBelow80, tempo80, tempo100, tempo120, tempo140, tempo160, tempoAbove200 };
    enum Feel { feelAny = 1, feelStraight, feelSwing, feelHalf, feelDouble };
}

//==============================================================================
class BrowserComponent::FolderItem : public juce::TreeViewItem
{
public:
    FolderItem (BrowserComponent& o, int f) : owner (o), folderIndex (f) {}

    const tt::FolderInfo& folder() const { return owner.index->folders[(size_t) folderIndex]; }
    int getFolderIndex() const { return folderIndex; }

    bool mightContainSubItems() override { return ! folder().children.empty(); }
    bool canBeSelected() const override { return ! folder().isHeading; }
    int getItemHeight() const override { return folder().isHeading ? 24 : 21; }

    juce::String getUniqueName() const override
    {
        return folderIndex == 0 ? juce::String ("root") : folder().rawName;
    }

    void itemOpennessChanged (bool isNowOpen) override
    {
        if (isNowOpen && getNumSubItems() == 0)
            for (auto c : folder().children)
                addSubItem (new FolderItem (owner, c));
    }

    FolderItem* openAndFindChild (int childFolder)
    {
        setOpen (true);
        for (int i = 0; i < getNumSubItems(); ++i)
            if (auto* it = dynamic_cast<FolderItem*> (getSubItem (i)); it != nullptr && it->folderIndex == childFolder)
                return it;
        return nullptr;
    }

    void paintItem (juce::Graphics& g, int w, int h) override
    {
        auto& f = folder();
        auto r = juce::Rectangle<int> (0, 0, w, h);

        if (f.isHeading)
        {
            g.setColour (theme::heading);
            g.setFont (theme::font (11.5f, true));
            g.drawText (f.name.toUpperCase(), r.reduced (2, 0).withTrimmedTop (6), juce::Justification::centredLeft, true);
            return;
        }

        auto count = juce::String (f.numGrooves);
        g.setFont (theme::font (11.0f));
        auto countW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), count)) + 8;
        g.setColour (theme::dim);
        g.drawText (count, r.removeFromRight (countW), juce::Justification::centredRight, false);

        g.setColour (isSelected() ? juce::Colours::white : (f.isLibrary || folderIndex == 0 ? theme::text : theme::text.withAlpha (0.88f)));
        g.setFont (theme::font (13.0f, f.isLibrary || folderIndex == 0));
        g.drawText (f.name, r.reduced (2, 0), juce::Justification::centredLeft, true);
    }

    void itemSelectionChanged (bool isNowSelected) override
    {
        if (isNowSelected)
            owner.showFolder (folderIndex);
    }

    void itemDoubleClicked (const juce::MouseEvent&) override { setOpen (! isOpen()); }

private:
    BrowserComponent& owner;
    int folderIndex;
};

//==============================================================================
BrowserComponent::BrowserComponent (GroovePlayerProcessor& p)
    : proc (p), app (*p.app)
{
    tree.setRootItemVisible (true);
    tree.setDefaultOpenness (false);
    tree.setIndentSize (14);
    tree.setColour (juce::TreeView::backgroundColourId, theme::panel);
    addAndMakeVisible (tree);

    searchBox.setTextToShowWhenEmpty ("Search grooves, styles, sections...", theme::dim);
    searchBox.setFont (theme::font (14.0f));
    searchBox.setIndents (8, 6);
    searchBox.onTextChange = [this] { startTimer (180); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); runSearch(); };
    searchBox.onReturnKey = [this] { stopTimer(); runSearch(); };
    addAndMakeVisible (searchBox);

    typeBox.addItemList ({ "Beats + Fills", "Beats", "Fills" }, 1);
    feelBox.addItemList ({ "Any feel", "Straight", "Swing / Shuffle", "Half time", "Double time" }, 1);
    tempoBox.addItemList ({ "Any tempo", "Near DAW tempo", "< 80 BPM", "80-100", "100-120", "120-140", "140-160", "160-200", "> 200 BPM" }, 1);

    for (auto* b : { &typeBox, &feelBox, &meterBox, &tempoBox, &styleBox })
    {
        b->setSelectedId (1, juce::dontSendNotification);
        b->onChange = [this] { runSearch(); };
        addAndMakeVisible (*b);
    }

    favButton.setClickingTogglesState (true);
    favButton.setTooltip ("Show favourite grooves only (click the star in the list to add one)");
    favButton.onClick = [this] { runSearch(); };
    scopeButton.setClickingTogglesState (true);
    scopeButton.setTooltip ("Limit search and filters to the selected folder");
    scopeButton.onClick = [this] { runSearch(); };
    clearButton.onClick = [this] { resetFilters(); };
    beatButton.setClickingTogglesState (true);
    beatButton.setTooltip ("Program a kick / snare / cymbal beat and find the grooves that match it best");
    beatButton.onClick = [this]
    {
        beatPanel.setVisible (beatButton.getToggleState());
        proc.uiState.setProperty ("beatSearchOpen", beatButton.getToggleState(), nullptr);
        resized();
        runSearch();
    };

    for (auto* b : { &favButton, &scopeButton, &clearButton, &beatButton })
        addAndMakeVisible (*b);

    beatPanel.fromString (proc.uiState.getProperty ("beatPattern").toString());
    beatPanel.onPatternChanged = [this]
    {
        proc.uiState.setProperty ("beatPattern", beatPanel.toString(), nullptr);

        // Hear edits immediately while the pattern is playing.
        if (isPatternPlaying())
        {
            if (beatPanel.hasHits())
                proc.startPreview (beatPanel.makeGroove(), true);
            else
                proc.stopPreview();
        }

        startTimer (150);
    };
    beatPanel.onPlay = [this]
    {
        if (isPatternPlaying())
        {
            proc.stopPreview();
        }
        else if (beatPanel.hasHits())
        {
            patternPreviewId = proc.startPreview (beatPanel.makeGroove());
            previewingGroove = -1;
            table.repaint();
        }
        beatPanel.setPlaying (isPatternPlaying());
    };
    addChildComponent (beatPanel);
    beatButton.setToggleState ((bool) proc.uiState.getProperty ("beatSearchOpen", false), juce::dontSendNotification);
    beatPanel.setVisible (beatButton.getToggleState());

    countLabel.setColour (juce::Label::textColourId, theme::dim);
    countLabel.setFont (theme::font (12.0f));
    countLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (countLabel);

    statusLabel.setColour (juce::Label::textColourId, theme::dim);
    statusLabel.setFont (theme::font (11.5f));
    addAndMakeVisible (statusLabel);

    folderLabel.setFont (theme::font (14.0f, true));
    folderLabel.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (folderLabel);

    table.setModel (this);
    table.setRowHeight (22);
    table.setMultipleSelectionEnabled (false);
    table.setColour (juce::ListBox::backgroundColourId, theme::row);
    table.setColour (juce::ListBox::outlineColourId, theme::outline);
    table.setOutlineThickness (1);

    auto& h = table.getHeader();
    using F = juce::TableHeaderComponent;
    h.addColumn ("", colFav, 26, 26, 26, F::notResizableOrSortable);
    h.addColumn ("Name", colName, 210, 80, 600, F::defaultFlags);
    h.addColumn ("Meter", colMeter, 52, 40, 90, F::defaultFlags);
    h.addColumn ("BPM", colTempo, 48, 40, 90, F::defaultFlags);
    h.addColumn ("Length", colLength, 62, 50, 110, F::defaultFlags);
    h.addColumn ("Type", colType, 150, 60, 400, F::defaultFlags);
    h.addColumn ("Location", colLocation, 300, 80, 1000, F::defaultFlags);
    h.addColumn ("Match", colMatch, 70, 50, 120, F::defaultFlags);
    h.setColumnVisible (colMatch, false);
    h.setStretchToFitActive (true);
    h.setPopupMenuActive (true);
    addAndMakeVisible (table);

    pattern.onDragOut = [this] (const juce::MouseEvent&) { startDragOfGroove (pattern.getGrooveIndex(), &pattern); };
    pattern.onDoubleClick = [this] { if (onAddToSong && selectedGroove >= 0) onAddToSong (selectedGroove); };
    addAndMakeVisible (pattern);

    infoTitle.setFont (theme::font (15.0f, true));
    infoTitle.setColour (juce::Label::textColourId, theme::text);
    infoMeta.setFont (theme::font (12.5f));
    infoMeta.setColour (juce::Label::textColourId, theme::dim);
    infoMeta.setJustificationType (juce::Justification::topLeft);
    infoMeta.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (infoTitle);
    addAndMakeVisible (infoMeta);

    playButton.setTooltip ("Preview the selected groove (loops, follows the DAW tempo)");
    playButton.onClick = [this] { if (selectedGroove >= 0) togglePreview (selectedGroove); };
    addButton.setTooltip ("Append the selected groove to the end of the song");
    addButton.onClick = [this] { if (onAddToSong && selectedGroove >= 0) onAddToSong (selectedGroove); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (addButton);

    autoPreviewToggle.setToggleState (app.getAutoPreview(), juce::dontSendNotification);
    autoPreviewToggle.setTooltip ("Start previewing as soon as a groove is selected");
    autoPreviewToggle.onClick = [this] { app.setAutoPreview (autoPreviewToggle.getToggleState()); };
    origTempoToggle.setToggleState (proc.engine.previewAtOriginalTempo.load(), juce::dontSendNotification);
    origTempoToggle.setTooltip ("When the DAW is stopped, preview at the groove's own tempo instead of the DAW tempo");
    origTempoToggle.onClick = [this] { proc.setPreviewAtOriginalTempo (origTempoToggle.getToggleState()); };
    addAndMakeVisible (autoPreviewToggle);
    addAndMakeVisible (origTempoToggle);

    app.addChangeListener (this);
    libraryChanged();
}

BrowserComponent::~BrowserComponent()
{
    app.removeChangeListener (this);
    tree.setRootItem (nullptr);
    table.setModel (nullptr);
}

//==============================================================================
void BrowserComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto treeArea = tree.getBounds().expanded (1);
    g.setColour (theme::outline);
    g.drawRect (treeArea);
}

void BrowserComponent::resized()
{
    auto r = getLocalBounds();

    auto left = r.removeFromLeft (juce::jlimit (220, 340, getWidth() / 4));
    statusLabel.setBounds (left.removeFromBottom (20).reduced (2, 0));
    tree.setBounds (left.reduced (1));
    r.removeFromLeft (8);

    auto bar1 = r.removeFromTop (30);
    clearButton.setBounds (bar1.removeFromRight (56));
    bar1.removeFromRight (4);
    scopeButton.setBounds (bar1.removeFromRight (78));
    bar1.removeFromRight (4);
    favButton.setBounds (bar1.removeFromRight (88));
    bar1.removeFromRight (4);
    beatButton.setBounds (bar1.removeFromRight (96));
    bar1.removeFromRight (6);
    searchBox.setBounds (bar1);
    r.removeFromTop (5);

    auto bar2 = r.removeFromTop (26);
    auto boxW = juce::jmax (90, (bar2.getWidth() - 4 * 5) / 5);
    for (auto* b : { &typeBox, &feelBox, &meterBox, &tempoBox, &styleBox })
    {
        b->setBounds (bar2.removeFromLeft (boxW));
        bar2.removeFromLeft (5);
    }
    r.removeFromTop (6);

    if (beatPanel.isVisible())
    {
        beatPanel.setBounds (r.removeFromTop (124));
        r.removeFromTop (6);
    }

    auto bar3 = r.removeFromTop (22);
    countLabel.setBounds (bar3.removeFromRight (200));
    folderLabel.setBounds (bar3);
    r.removeFromTop (2);

    auto info = r.removeFromBottom (juce::jlimit (120, 170, getHeight() / 3));
    r.removeFromBottom (6);
    table.setBounds (r);

    auto side = info.removeFromLeft (juce::jmin (250, info.getWidth() / 3));
    infoTitle.setBounds (side.removeFromTop (22));
    auto buttons = side.removeFromBottom (24);
    playButton.setBounds (buttons.removeFromLeft (70));
    buttons.removeFromLeft (6);
    addButton.setBounds (buttons.removeFromLeft (100));
    auto toggles = side.removeFromBottom (22);
    autoPreviewToggle.setBounds (toggles.removeFromLeft (side.getWidth() / 2));
    origTempoToggle.setBounds (toggles);
    infoMeta.setBounds (side.reduced (0, 2));
    info.removeFromLeft (8);
    pattern.setBounds (info);
}

//==============================================================================
void BrowserComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (app.getIndex() != index)
        libraryChanged();
    else
        statusLabel.setText (app.getStatusText(), juce::dontSendNotification);

    if (app.getRhythmIndex() != rhythm)
    {
        rhythm = app.getRhythmIndex();
        if (beatSearchActive())
            runSearch();
    }
}

void BrowserComponent::libraryChanged()
{
    index = app.getIndex();
    tree.setRootItem (nullptr);
    rootItem.reset();
    rows.clear();
    selectedGroove = -1;
    statusLabel.setText (app.getStatusText(), juce::dontSendNotification);

    meterBox.clear (juce::dontSendNotification);
    meterBox.addItem ("Any meter", 1);
    styleBox.clear (juce::dontSendNotification);
    styleBox.addItem ("Any play style", 1);

    if (index == nullptr || index->folders.empty())
    {
        table.updateContent();
        return;
    }

    for (int i = 0; i < index->timeSignatures.size(); ++i)
        meterBox.addItem (index->timeSignatures[i], i + 2);
    for (int i = 0; i < index->playStyles.size(); ++i)
        styleBox.addItem (index->playStyles[i], i + 2);
    meterBox.setSelectedId (1, juce::dontSendNotification);
    styleBox.setSelectedId (1, juce::dontSendNotification);

    rootItem = std::make_unique<FolderItem> (*this, 0);
    tree.setRootItem (rootItem.get());
    rootItem->setOpen (true);

    restoreSelectedFolder();
}

void BrowserComponent::restoreSelectedFolder()
{
    auto path = proc.uiState.getProperty ("folder").toString();
    auto target = path.isNotEmpty() ? index->findFolderByPath (path) : -1;

    if (target <= 0)
    {
        rootItem->setSelected (true, true);
        return;
    }

    std::vector<int> chain;
    for (int f = target; f > 0; f = index->folders[(size_t) f].parent)
        chain.insert (chain.begin(), f);

    FolderItem* item = rootItem.get();
    for (auto f : chain)
        if (item != nullptr)
            item = item->openAndFindChild (f);

    if (item != nullptr)
    {
        item->setSelected (true, true);
        tree.scrollToKeepItemVisible (item);
    }
    else
    {
        rootItem->setSelected (true, true);
    }
}

void BrowserComponent::showFolder (int folder)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (folder, (int) index->folders.size()))
        return;

    currentFolder = folder;
    proc.uiState.setProperty ("folder", index->folders[(size_t) folder].path, nullptr);
    folderLabel.setText (folder == 0 ? juce::String ("All Libraries") : index->getFolderPathText (folder), juce::dontSendNotification);

    if (filtersActive() && ! scopeButton.getToggleState())
        return; // a library-wide search stays visible while browsing

    runSearch();
}

bool BrowserComponent::filtersActive() const
{
    return makeFilter().isActive() || beatSearchActive();
}

tt::SearchFilter BrowserComponent::makeFilter() const
{
    tt::SearchFilter f;
    if (index == nullptr)
        return f;

    f.text = searchBox.getText();
    f.scopeFolder = scopeButton.getToggleState() ? currentFolder : 0;
    f.type = typeBox.getSelectedId() == 2 ? tt::SearchFilter::Type::beats
           : typeBox.getSelectedId() == 3 ? tt::SearchFilter::Type::fills
                                          : tt::SearchFilter::Type::any;

    switch (feelBox.getSelectedId())
    {
        case feelStraight: f.requiredTags |= index->tagMask ("Straight"); break;
        case feelSwing:    f.requiredTags |= index->tagMask ("Swing"); break;
        case feelHalf:     f.requiredTags |= index->tagMask ("Half Time"); break;
        case feelDouble:   f.requiredTags |= index->tagMask ("Double Time"); break;
        default: break;
    }

    if (styleBox.getSelectedId() > 1)
        f.requiredTags |= index->tagMask (styleBox.getText());

    if (meterBox.getSelectedId() > 1)
    {
        auto sig = meterBox.getText();
        f.num = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        f.den = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
    }

    switch (tempoBox.getSelectedId())
    {
        case tempoNearHost:
        {
            auto t = proc.engine.currentTempo.load();
            f.tempoMin = t * 0.92;
            f.tempoMax = t * 1.08;
            break;
        }
        case tempoBelow80:  f.tempoMax = 79.99; break;
        case tempo80:       f.tempoMin = 80;  f.tempoMax = 99.99; break;
        case tempo100:      f.tempoMin = 100; f.tempoMax = 119.99; break;
        case tempo120:      f.tempoMin = 120; f.tempoMax = 139.99; break;
        case tempo140:      f.tempoMin = 140; f.tempoMax = 159.99; break;
        case tempo160:      f.tempoMin = 160; f.tempoMax = 200; break;
        case tempoAbove200: f.tempoMin = 200.01; break;
        default: break;
    }

    f.favouritesOnly = favButton.getToggleState();
    return f;
}

void BrowserComponent::resetFilters()
{
    searchBox.clear();
    for (auto* b : { &typeBox, &feelBox, &meterBox, &tempoBox, &styleBox })
        b->setSelectedId (1, juce::dontSendNotification);
    favButton.setToggleState (false, juce::dontSendNotification);
    scopeButton.setToggleState (false, juce::dontSendNotification);
    runSearch();
}

void BrowserComponent::runSearch()
{
    stopTimer();
    if (index == nullptr)
        return;

    auto filter = makeFilter();
    rows.clear();
    table.getHeader().setSortColumnId (0, true);

    matchScores.clear();
    beatResults = false;

    if (beatSearchActive())
    {
        // Beat search: score the (filtered) candidates against the programmed pattern.
        searchMode = true;
        showLocation = true;
        beatResults = true;
        rhythm = app.getRhythmIndex();

        std::vector<int> candidates;
        if (filter.isActive())
        {
            candidates = index->search (filter, [this] (int i) { return app.isFavourite (i); });
        }
        else
        {
            auto& sf = index->folders[(size_t) (scopeButton.getToggleState() ? currentFolder : 0)];
            for (int i = sf.descBegin; i < sf.descEnd; ++i)
                if (! index->grooves[(size_t) i].isHeading)
                    candidates.push_back (i);
        }

        if (rhythm != nullptr && rhythm->library == index)
        {
            for (auto& m : rhythm->search (beatPanel.getPattern(), beatPanel.getNum(), beatPanel.getDen(), candidates, 0.35f, 2000))
            {
                rows.push_back (m.groove);
                matchScores[m.groove] = m.score;
            }
        }
    }
    else if (filter.isActive())
    {
        searchMode = true;
        rows = index->search (filter, [this] (int i) { return app.isFavourite (i); });
        showLocation = true;
    }
    else
    {
        searchMode = false;
        auto& f = index->folders[(size_t) currentFolder];

        if (f.numFiles > 0)
        {
            for (int i = f.firstFile; i < f.firstFile + f.numFiles; ++i)
                rows.push_back (i);
            showLocation = false;
        }
        else
        {
            // A category folder: list everything below it.
            for (int i = f.descBegin; i < f.descEnd; ++i)
                if (! index->grooves[(size_t) i].isHeading)
                    rows.push_back (i);
            showLocation = true;
        }
    }

    int playable = 0;
    for (auto i : rows)
        playable += index->grooves[(size_t) i].isHeading ? 0 : 1;

    if (beatResults && (rhythm == nullptr || rhythm->library != index))
        countLabel.setText ("Beat search: analysing library...", juce::dontSendNotification);
    else if (beatResults)
        countLabel.setText (juce::String (playable) + (playable == 1 ? " match" : " matches") + " (best first)",
                            juce::dontSendNotification);
    else
        countLabel.setText (juce::String (playable) + (playable == 1 ? " groove" : " grooves") + (searchMode ? " found" : ""),
                            juce::dontSendNotification);

    updateColumns();
    table.deselectAllRows();
    table.updateContent();
    table.scrollToEnsureRowIsOnscreen (0);
    table.repaint();
}

void BrowserComponent::updateColumns()
{
    table.getHeader().setColumnVisible (colLocation, showLocation);
    table.getHeader().setColumnVisible (colMatch, beatResults);
}

void BrowserComponent::sortOrderChanged (int col, bool forwards)
{
    if (index == nullptr || col == 0)
        return;

    rows.erase (std::remove_if (rows.begin(), rows.end(), [this] (int i) { return index->grooves[(size_t) i].isHeading; }), rows.end());

    auto& gs = index->grooves;
    auto key = [&] (int i) -> double
    {
        auto& g = gs[(size_t) i];
        switch (col)
        {
            case colMeter:  return g.den > 0 ? g.den * 100.0 + g.num : 1.0e9;
            case colTempo:  return g.tempo > 0 ? g.tempo : 1.0e9;
            case colLength: return g.lengthBeats > 0 ? g.lengthBeats : 1.0e9;
            default:        return 0.0;
        }
    };

    std::stable_sort (rows.begin(), rows.end(), [&] (int a, int b)
    {
        int c = 0;
        if (col == colName)          c = gs[(size_t) a].name.compareNatural (gs[(size_t) b].name);
        else if (col == colType)     c = index->getTypeText (gs[(size_t) a]).compare (index->getTypeText (gs[(size_t) b]));
        else if (col == colLocation) c = a < b ? -1 : (a > b ? 1 : 0); // index order == library order
        else if (col == colMatch)    c = matchScores[a] > matchScores[b] ? -1 : (matchScores[a] < matchScores[b] ? 1 : 0);
        else                         c = key (a) < key (b) ? -1 : (key (a) > key (b) ? 1 : 0);
        return forwards ? c < 0 : c > 0;
    });

    table.updateContent();
    table.repaint();
}

//==============================================================================
juce::String BrowserComponent::lengthText (const tt::GrooveInfo& g) const
{
    if (g.lengthBeats <= 0.0f)
        return "-";

    auto barLen = g.num > 0 && g.den > 0 ? g.num * 4.0 / g.den : 4.0;
    auto bars = g.lengthBeats / barLen;

    if (bars < 0.99)
    {
        auto beats = juce::roundToInt (g.lengthBeats);
        return juce::String (beats) + (beats == 1 ? " beat" : " beats");
    }

    auto rounded = std::round (bars * 4.0) / 4.0;
    auto txt = std::abs (rounded - std::round (rounded)) < 1.0e-6 ? juce::String ((int) std::round (rounded))
                                                                   : juce::String (rounded, 2);
    return txt + (std::abs (rounded - 1.0) < 1.0e-6 ? " bar" : " bars");
}

void BrowserComponent::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    auto& gi = index->grooves[(size_t) rows[(size_t) row]];

    if (gi.isHeading)
    {
        g.fillAll (theme::panel);
        g.setColour (theme::heading);
        g.setFont (theme::font (11.5f, true));
        g.drawText (gi.name.toUpperCase(), 30, 0, w - 36, h, juce::Justification::centredLeft, true);
        return;
    }

    g.fillAll (selected ? theme::selection : (row % 2 ? theme::rowAlt : theme::row));

    if (rows[(size_t) row] == previewingGroove && proc.isPreviewing())
    {
        g.setColour (theme::accent);
        g.fillRect (0, 0, 3, h);
    }
}

void BrowserComponent::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool selected)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    auto gIndex = rows[(size_t) row];
    auto& gi = index->grooves[(size_t) gIndex];
    if (gi.isHeading)
        return;

    auto textColour = selected ? juce::Colours::white : theme::text;
    g.setFont (theme::font (13.0f));

    switch (col)
    {
        case colFav:
        {
            auto fav = app.isFavourite (gIndex);
            auto star = starPath (juce::Rectangle<float> (0, 0, (float) w, (float) h).withSizeKeepingCentre (13.0f, 13.0f));
            if (fav) { g.setColour (theme::accent); g.fillPath (star); }
            else     { g.setColour (theme::outline.brighter (0.2f)); g.strokePath (star, juce::PathStrokeType (1.0f)); }
            break;
        }
        case colName:
        {
            auto r = juce::Rectangle<int> (4, 0, w - 8, h);
            if (gIndex == previewingGroove && proc.isPreviewing())
            {
                juce::Path p;
                p.addTriangle (4, h * 0.5f - 4, 4, h * 0.5f + 4, 11, h * 0.5f);
                g.setColour (theme::accent);
                g.fillPath (p);
                r.removeFromLeft (12);
            }
            g.setColour (textColour);
            g.drawText (gi.name, r, juce::Justification::centredLeft, true);
            break;
        }
        case colMeter:
            g.setColour (textColour.withAlpha (0.8f));
            g.drawText (gi.num > 0 ? juce::String (gi.num) + "/" + juce::String (gi.den) : juce::String ("-"),
                        4, 0, w - 8, h, juce::Justification::centredLeft, true);
            break;
        case colTempo:
            g.setColour (textColour.withAlpha (0.8f));
            g.drawText (gi.tempo > 0 ? juce::String (juce::roundToInt (gi.tempo)) : juce::String ("-"),
                        4, 0, w - 8, h, juce::Justification::centredLeft, true);
            break;
        case colLength:
            g.setColour (textColour.withAlpha (0.8f));
            g.drawText (lengthText (gi), 4, 0, w - 8, h, juce::Justification::centredLeft, true);
            break;
        case colType:
            g.setColour (selected ? juce::Colours::white.withAlpha (0.8f) : theme::dim);
            g.setFont (theme::font (12.0f));
            g.drawText (index->getTypeText (gi), 4, 0, w - 8, h, juce::Justification::centredLeft, true);
            break;
        case colMatch:
        {
            auto it = matchScores.find (gIndex);
            if (it == matchScores.end())
                break;

            auto bar = juce::Rectangle<float> (4.0f, h * 0.5f - 3.0f, (float) (w - 40), 6.0f);
            g.setColour (theme::row.darker (0.3f));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (theme::accent.withAlpha (0.4f + 0.6f * it->second));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * it->second), 3.0f);
            g.setColour (textColour);
            g.setFont (theme::font (12.0f));
            g.drawText (juce::String (juce::roundToInt (it->second * 100.0f)) + "%", w - 36, 0, 34, h,
                        juce::Justification::centredRight, false);
            break;
        }
        case colLocation:
            g.setColour (selected ? juce::Colours::white.withAlpha (0.8f) : theme::dim);
            g.setFont (theme::font (12.0f));
            g.drawText (index->getLocation (gIndex), 4, 0, w - 8, h, juce::Justification::centredLeft, true);
            break;
        default:
            break;
    }
}

juce::String BrowserComponent::getCellTooltip (int row, int col)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
        return {};

    auto gIndex = rows[(size_t) row];
    if (col == colFav)
        return app.isFavourite (gIndex) ? "Remove from favourites" : "Add to favourites";

    return index->getFile (gIndex).getFullPathName();
}

void BrowserComponent::selectedRowsChanged (int lastRow)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (lastRow, (int) rows.size()))
        return;

    auto gIndex = rows[(size_t) lastRow];
    if (index->grooves[(size_t) gIndex].isHeading)
        return;

    suppressNextClick = true;
    selectGroove (gIndex, autoPreviewToggle.getToggleState());
}

void BrowserComponent::cellClicked (int row, int col, const juce::MouseEvent&)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    auto gIndex = rows[(size_t) row];
    if (index->grooves[(size_t) gIndex].isHeading)
        return;

    if (col == colFav)
    {
        app.toggleFavourite (gIndex);
        table.repaintRow (row);
        if (favButton.getToggleState())
            runSearch();
        suppressNextClick = false;
        return;
    }

    // Clicking the already selected row toggles its preview.
    if (! suppressNextClick)
        togglePreview (gIndex);

    suppressNextClick = false;
}

void BrowserComponent::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    addRowToSong (row);
}

void BrowserComponent::addRowToSong (int row)
{
    if (index != nullptr && juce::isPositiveAndBelow (row, (int) rows.size()) && onAddToSong)
        if (! index->grooves[(size_t) rows[(size_t) row]].isHeading)
            onAddToSong (rows[(size_t) row]);
}

void BrowserComponent::returnKeyPressed (int row)
{
    addRowToSong (row);
}

juce::var BrowserComponent::getDragSourceDescription (const juce::SparseSet<int>& selected)
{
    if (index == nullptr || selected.isEmpty())
        return {};

    auto row = selected[0];
    if (! juce::isPositiveAndBelow (row, (int) rows.size()) || index->grooves[(size_t) rows[(size_t) row]].isHeading)
        return {};

    return drag::make (drag::groove, rows[(size_t) row]);
}

void BrowserComponent::startDragOfGroove (int grooveIndex, juce::Component* source)
{
    if (grooveIndex < 0)
        return;

    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        auto img = source->createComponentSnapshot (source->getLocalBounds(), true, 0.6f);
        c->startDragging (drag::make (drag::groove, grooveIndex), source, juce::ScaledImage (img, 1.0), false);
    }
}

//==============================================================================
void BrowserComponent::selectGroove (int gIndex, bool preview)
{
    if (index == nullptr)
        return;

    selectedGroove = gIndex;
    auto& gi = index->grooves[(size_t) gIndex];
    infoTitle.setText (gi.name, juce::dontSendNotification);
    updateInfo (nullptr);

    if (pattern.getGrooveIndex() != gIndex)
        pattern.setGroove (nullptr, -1);
    pattern.setMessage ("Loading...");

    juce::Component::SafePointer<BrowserComponent> safe (this);
    app.loadGrooveAsync (gIndex, [safe, gIndex, preview] (tt::Groove::Ptr g, juce::String err)
    {
        if (safe == nullptr || safe->selectedGroove != gIndex)
            return;

        if (g == nullptr)
        {
            safe->pattern.setGroove (nullptr, -1);
            safe->pattern.setMessage (err.isNotEmpty() ? err : juce::String ("Could not load groove"));
            return;
        }

        safe->pattern.setGroove (g, gIndex);
        safe->updateInfo (g.get());

        if (preview)
        {
            safe->proc.startPreview (g);
            safe->previewingGroove = gIndex;
            safe->table.repaint();
        }
    });
}

void BrowserComponent::togglePreview (int gIndex)
{
    if (proc.isPreviewing() && previewingGroove == gIndex)
    {
        proc.stopPreview();
        previewingGroove = -1;
        table.repaint();
        return;
    }

    selectGroove (gIndex, true);
}

void BrowserComponent::updateInfo (const tt::Groove* g)
{
    if (index == nullptr || selectedGroove < 0)
    {
        infoMeta.setText ({}, juce::dontSendNotification);
        return;
    }

    auto gi = index->grooves[(size_t) selectedGroove];
    if (g != nullptr)
    {
        if (gi.lengthBeats <= 0.0f) gi.lengthBeats = (float) g->lengthBeats;
        if (gi.tempo <= 0.0f)       gi.tempo = (float) g->tempo;
        if (gi.num == 0 && g->hasTimeSig) { gi.num = (juce::uint8) g->num; gi.den = (juce::uint8) g->den; }
    }

    juce::StringArray parts;
    if (gi.num > 0) parts.add (juce::String (gi.num) + "/" + juce::String (gi.den));
    if (gi.tempo > 0) parts.add (juce::String (juce::roundToInt (gi.tempo)) + " BPM");
    parts.add (lengthText (gi));

    auto text = parts.joinIntoString ("  -  ");
    auto type = index->getTypeText (gi);
    if (type.isNotEmpty())
        text << "\n" << type;
    if (g != nullptr)
        text << "\n" << juce::String ((int) g->notes.size()) << " hits";
    text << "\n" << index->getLocation (selectedGroove);

    infoMeta.setText (text, juce::dontSendNotification);
}

void BrowserComponent::updatePlayState()
{
    auto previewing = proc.isPreviewing();
    if (! previewing && previewingGroove >= 0)
    {
        previewingGroove = -1;
        table.repaint();
    }

    playButton.setButtonText (previewing && previewingGroove == selectedGroove ? "Stop" : "Play");
    playButton.setToggleState (previewing && previewingGroove == selectedGroove, juce::dontSendNotification);

    if (previewing && pattern.getGroove() == proc.getPreviewGroove())
        pattern.setPlayhead (proc.engine.previewPosition.load());
    else
        pattern.setPlayhead (-1.0);

    beatPanel.setPlaying (isPatternPlaying());

    if (beatPanel.isVisible())
        beatPanel.setStatus (app.isIndexingRhythms() || app.getRhythmIndex() == nullptr
                                 ? app.getRhythmStatusText()
                                 : juce::String ("Click / drag cells to program a beat"));

    auto scanning = app.isScanning();
    if (scanning || wasScanning)
        statusLabel.setText (app.getStatusText(), juce::dontSendNotification);
    wasScanning = scanning;
}

void BrowserComponent::timerCallback()
{
    runSearch();
}
