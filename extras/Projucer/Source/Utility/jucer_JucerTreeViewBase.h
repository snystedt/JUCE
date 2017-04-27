/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2015 - ROLI Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

#pragma once

class ProjectContentComponent;
class Project;

//==============================================================================
class JucerTreeViewBase   : public TreeViewItem
{
public:
    JucerTreeViewBase();
    ~JucerTreeViewBase();

    int getItemWidth() const override                   { return -1; }
    int getItemHeight() const override                  { return 25; }

    void paintOpenCloseButton (Graphics&, const Rectangle<float>& area, Colour backgroundColour, bool isMouseOver) override;
    void paintItem (Graphics& g, int width, int height) override;
    void itemClicked (const MouseEvent& e) override;
    void itemSelectionChanged (bool isNowSelected) override;
    void itemDoubleClicked (const MouseEvent&) override;

    void cancelDelayedSelectionTimer();

    //==============================================================================
    virtual bool isRoot() const                                 { return false; }
    virtual Font getFont() const;
    virtual String getRenamingName() const = 0;
    virtual String getDisplayName() const = 0;
    virtual void setName (const String& newName) = 0;
    virtual bool isMissing() const = 0;
    virtual Icon getIcon() const = 0;
    virtual bool isIconCrossedOut() const                         { return false; }
    virtual void paintIcon (Graphics& g, Rectangle<float> area);
    virtual void paintContent (Graphics& g, const Rectangle<int>& area);
    virtual int getRightHandButtonSpace() { return 0; }
    virtual Colour getContentColour (bool isIcon) const;
    virtual int getMillisecsAllowedForDragGesture()             { return 120; }
    virtual File getDraggableFile() const                       { return {}; }
    virtual Component* createItemComponent() override;

    void refreshSubItems();
    virtual void deleteItem();
    virtual void deleteAllSelectedItems();
    virtual void showDocument();
    virtual void showMultiSelectionPopupMenu();
    virtual void showRenameBox();

    void launchPopupMenu (PopupMenu&); // runs asynchronously, and produces a callback to handlePopupMenuResult().
    virtual void showPopupMenu();
    virtual void showPlusMenu();
    virtual void handlePopupMenuResult (int resultCode);

    //==============================================================================
    // To handle situations where an item gets deleted before openness is
    // restored for it, this OpennessRestorer keeps only a pointer to the
    // topmost tree item.
    struct WholeTreeOpennessRestorer   : public OpennessRestorer
    {
        WholeTreeOpennessRestorer (TreeViewItem& item)  : OpennessRestorer (getTopLevelItem (item))
        {}

    private:
        static TreeViewItem& getTopLevelItem (TreeViewItem& item)
        {
            if (TreeViewItem* const p = item.getParentItem())
                return getTopLevelItem (*p);

            return item;
        }
    };

    int textX;

protected:
    ProjectContentComponent* getProjectContentComponent() const;
    virtual void addSubItems() {}

private:
    class ItemSelectionTimer;
    friend class ItemSelectionTimer;
    ScopedPointer<Timer> delayedSelectionTimer;

    WeakReference<JucerTreeViewBase>::Master masterReference;
    friend class WeakReference<JucerTreeViewBase>;

    void invokeShowDocument();
};

//==============================================================================
class TreePanelBase   : public Component
{
public:
    TreePanelBase (const Project* p, const String& treeviewID)
        : project (p), opennessStateKey (treeviewID)
    {
        addAndMakeVisible (tree);

        tree.setRootItemVisible (true);
        tree.setDefaultOpenness (true);
        tree.setColour (TreeView::backgroundColourId, Colours::transparentBlack);
        tree.setIndentSize (14);
        tree.getViewport()->setScrollBarThickness (6);

        tree.addMouseListener (this, true);
    }

    ~TreePanelBase()
    {
        tree.setRootItem (nullptr);
    }

    void setRoot (JucerTreeViewBase*);
    void saveOpenness();

    virtual void deleteSelectedItems()
    {
        if (rootItem != nullptr)
            rootItem->deleteAllSelectedItems();
    }

    void setEmptyTreeMessage (const String& newMessage)
    {
        if (emptyTreeMessage != newMessage)
        {
            emptyTreeMessage = newMessage;
            repaint();
        }
    }

    static void drawEmptyPanelMessage (Component& comp, Graphics& g, const String& message)
    {
        const int fontHeight = 13;
        const Rectangle<int> area (comp.getLocalBounds());
        g.setColour (comp.findColour (defaultTextColourId));
        g.setFont ((float) fontHeight);
        g.drawFittedText (message, area.reduced (4, 2), Justification::centred, area.getHeight() / fontHeight);
    }

    void paint (Graphics& g) override
    {
        if (emptyTreeMessage.isNotEmpty() && (rootItem == nullptr || rootItem->getNumSubItems() == 0))
            drawEmptyPanelMessage (*this, g, emptyTreeMessage);
    }

    void resized() override
    {
        tree.setBounds (getAvailableBounds());
    }

    Rectangle<int> getAvailableBounds() const
    {
        return Rectangle<int> (0, 2, getWidth() - 2, getHeight() - 2);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.eventComponent == &tree)
            tree.clearSelectedItems();
    }

    const Project* project;
    TreeView tree;
    ScopedPointer<JucerTreeViewBase> rootItem;

private:
    String opennessStateKey, emptyTreeMessage;
};

//==============================================================================
class TreeItemComponent   : public Component
{
public:
    TreeItemComponent (JucerTreeViewBase& i)  : item (i)
    {
        setInterceptsMouseClicks (false, true);
    }

    void paint (Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto iconBounds = bounds.removeFromLeft (25).reduced (7, 5);

        bounds.removeFromRight (buttons.size() * bounds.getHeight());

        item.paintIcon    (g, iconBounds);
        item.paintContent (g, bounds.toNearestInt());
    }

    void resized() override
    {
        item.textX = getHeight() + 4;

        Rectangle<int> r (getLocalBounds());

        for (int i = buttons.size(); --i >= 0;)
            buttons.getUnchecked(i)->setBounds (r.removeFromRight (r.getHeight()));
    }

    void addRightHandButton (Component* button)
    {
        buttons.add (button);
        addAndMakeVisible (button);
    }

    JucerTreeViewBase& item;
    OwnedArray<Component> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TreeItemComponent)
};
