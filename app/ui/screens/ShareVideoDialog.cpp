#include "ShareVideoDialog.h"

namespace vb
{
namespace
{
    constexpr int previewW = 380, labelW = 132, rowH = 40;
    constexpr int maxBackgroundSide = 2048;   // 背景の画像はこの大きさまで縮めて持つ（コマは最大 1920）
}

ShareVideoDialog::ShareVideoDialog (UiSession& u)
    : DialogPanel (tr ("share.title"), tr ("share.micro")),
      SessionView (u),
      shapeKeys ({ tr ("share.shape.portrait"), tr ("share.shape.square"), tr ("share.shape.landscape") }, 0),
      spanKeys ({ tr ("share.span.song"), tr ("share.span.range") }, 0),
      pitchKey (tr ("share.pitch.show")),
      chooseKey (tr ("share.background.choose")),
      clearKey (tr ("share.background.clear")),
      copyKey (tr ("share.post.copy")),
      openKey (tr ("share.openFolder")),
      cancelKey (tr ("share.cancel"))
{
    const auto& s = state();
    // 範囲を選んでいれば、はじめから範囲（サビだけなど）
    if (s.rangeOut > s.rangeIn)
        spanKeys.setSelected (1, juce::dontSendNotification);

    shapeKeys.setTitle (tr ("share.shape"));   // 読み上げの名前（#28）
    spanKeys.setTitle (tr ("share.span"));
    shapeKeys.onChange = [this] (int) { optionsChanged(); };
    spanKeys.onChange = [this] (int) { optionsChanged(); };
    addAndMakeVisible (shapeKeys);
    addAndMakeVisible (spanKeys);

    pitchKey.withLed().withToggle (true);
    pitchKey.setToggleState (true, juce::dontSendNotification);
    pitchKey.onClick = [this] { optionsChanged(); };
    addAndMakeVisible (pitchKey);

    chooseKey.onClick = [this] { chooseBackground(); };
    clearKey.onClick = [this]
    {
        background = {};
        backgroundName = backgroundError = {};
        optionsChanged();
    };
    addAndMakeVisible (chooseKey);
    addAndMakeVisible (clearKey);

    post.setMultiLine (true, true);
    post.setReturnKeyStartsNewLine (true);
    post.setIndents (10, 8);
    post.setFont (sans (12.0f));
    post.setText (session.sharePostText(), false);
    post.setTitle (tr ("share.post"));
    addAndMakeVisible (post);
    post.onTextChange = [this] { if (copied) { copied = false; repaint(); } };
    copyKey.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (post.getText());
        copied = true;
        repaint();
    };
    addAndMakeVisible (copyKey);

    openKey.onClick = [this]
    {
        const auto f = state().share.file;
        if (f.existsAsFile())
            f.revealToUser();
    };
    cancelKey.onClick = [this] { session.cancelShareVideo(); };
    addChildComponent (openKey);
    addChildComponent (cancelKey);

    exportKey = &addFooterKey (tr ("share.do"), KeyRole::primary, [this] { session.exportShareVideo (request()); });
    addFooterKey (tr ("common.close"), KeyRole::normal, [this] { if (onFinished) onFinished(); });
    onCloseRequest = [this] { if (onFinished) onFinished(); };

    setSize (1120, 780);
    refreshState();
    refreshPreview();
}

ShareVideoDialog::~ShareVideoDialog()
{
    chooser = nullptr;
}

UiSession::ShareRequest ShareVideoDialog::request() const
{
    UiSession::ShareRequest r;
    r.shape = (share::Shape) shapeKeys.getSelected();
    r.useRange = spanKeys.getSelected() == 1;
    r.showPitch = pitchKey.getToggleState();
    r.background = background;
    return r;
}

void ShareVideoDialog::optionsChanged()
{
    refreshPreview();
    refreshState();
}

void ShareVideoDialog::refreshPreview()
{
    // 見本のコマ：いまの位置が区間の中ならそこ、外なら区間の 4 割のところ（歌い出しの前の無音を避ける）
    const auto& s = state();
    const auto req = request();
    const auto scene = session.shareScene (req);
    auto t = s.playhead;
    if (t < scene.from || t >= scene.to)
        t = scene.from + (scene.to - scene.from) * 2 / 5;
    const auto still = share::paintStatic (scene);
    preview = juce::Image (juce::Image::ARGB, still.getWidth(), still.getHeight(), false, juce::SoftwareImageType());
    {
        juce::Graphics g (preview);
        share::paintFrame (g, scene, still, t);
    }
    repaint();
}

void ShareVideoDialog::refreshState()
{
    const auto& s = state();
    const auto& job = s.share;
    const bool can = session.canShareVideo();
    const bool hasRange = s.rangeOut > s.rangeIn;
    if (! hasRange && spanKeys.getSelected() == 1)
        spanKeys.setSelected (0, juce::dontSendNotification);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &shapeKeys, &spanKeys, &pitchKey, &chooseKey, &clearKey })
        c->setEnabled (! job.running);
    spanKeys.setEnabled (! job.running && hasRange);
    clearKey.setEnabled (! job.running && background.isValid());
    if (exportKey != nullptr)
        exportKey->setEnabled (can && ! job.running);
    cancelKey.setVisible (job.running);
    openKey.setVisible (! job.running && job.file.existsAsFile());
    resized();
    repaint();
}

void ShareVideoDialog::onSessionChanged (juce::uint32 changes)
{
    if ((changes & change::share) != 0)
        refreshState();
    if ((changes & (change::song | change::takes | change::songInfo | change::range)) != 0)
    {
        refreshState();
        refreshPreview();
    }
}

void ShareVideoDialog::chooseBackground()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("share.background.chooser"),
                                                   juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                   "*.png;*.jpg;*.jpeg");
    juce::Component::SafePointer<ShareVideoDialog> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f != juce::File())
                                  safe->loadBackground (f);
                          });
}

void ShareVideoDialog::loadBackground (const juce::File& f)
{
    auto img = juce::ImageFileFormat::loadFrom (f);
    if (! img.isValid())
    {
        backgroundError = tr ("share.background.readError", f.getFileName());
        repaint();
        return;
    }
    // 大きな写真はコマより少し大きいくらいまで縮める（毎回の描画とメモリを軽く）
    const auto side = juce::jmax (img.getWidth(), img.getHeight());
    if (side > maxBackgroundSide)
    {
        const auto k = (double) maxBackgroundSide / side;
        img = img.rescaled (juce::jmax (1, (int) (img.getWidth() * k)), juce::jmax (1, (int) (img.getHeight() * k)),
                            juce::Graphics::highResamplingQuality);
    }
    background = img.convertedToFormat (juce::Image::ARGB);
    backgroundName = f.getFileName();
    backgroundError = {};
    optionsChanged();
}

//==============================================================================
void ShareVideoDialog::layoutBody (juce::Rectangle<int> r)
{
    previewArea = r.removeFromLeft (previewW);
    r.removeFromLeft (28);

    auto keysIn = [] (juce::Rectangle<int> row) { return row.withTrimmedLeft (labelW); };

    shapeRow = r.removeFromTop (rowH);
    shapeKeys.setBounds (keysIn (shapeRow).withWidth (juce::jmin (keysIn (shapeRow).getWidth(), shapeKeys.idealWidth() + 40)).reduced (0, 4));
    r.removeFromTop (8);

    spanRow = r.removeFromTop (rowH + 26);   // 下の 1 行に長さと X の目安
    spanKeys.setBounds (keysIn (spanRow.withHeight (rowH)).withWidth (juce::jmin (keysIn (spanRow).getWidth(), spanKeys.idealWidth() + 40)).reduced (0, 4));
    r.removeFromTop (8);

    pitchRow = r.removeFromTop (rowH);
    pitchKey.setBounds (keysIn (pitchRow).withWidth (juce::jmax (150, pitchKey.idealWidth())).reduced (0, 4));
    r.removeFromTop (8);

    backRow = r.removeFromTop (rowH + 24);
    {
        auto keys = keysIn (backRow.withHeight (rowH)).reduced (0, 4);
        chooseKey.setBounds (keys.removeFromLeft (juce::jmax (150, chooseKey.idealWidth())));
        keys.removeFromLeft (8);
        clearKey.setBounds (keys.removeFromLeft (juce::jmax (130, clearKey.idealWidth())));
    }
    r.removeFromTop (10);

    postLabel = r.removeFromTop (30);
    copyKey.setBounds (postLabel.withLeft (postLabel.getRight() - juce::jmax (100, copyKey.idealWidth())).reduced (0, 1));
    post.setBounds (r.removeFromTop (112));
    r.removeFromTop (10);

    statusArea = r.removeFromBottom (40);
    {
        auto keys = statusArea;
        const auto w = juce::jmax (140, juce::jmax (openKey.idealWidth(), cancelKey.idealWidth()));
        auto k = keys.removeFromRight (w).reduced (0, 4);
        openKey.setBounds (k);
        cancelKey.setBounds (k);
    }
    r.removeFromBottom (6);
    rightsArea = r;
}

void ShareVideoDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    const auto& s = state();
    const auto& job = s.share;

    // 見本（実際の 1 コマを縮めて）
    {
        auto area = previewArea.toFloat();
        auto head = area.removeFromTop (18.0f);
        paint::microLabel (g, head, tr ("share.preview"), colours::textMute);
        area.removeFromTop (6.0f);
        paint::inset (g, area);
        if (preview.isValid())
        {
            const auto box = area.reduced (10.0f);
            const auto k = juce::jmin (box.getWidth() / (float) preview.getWidth(), box.getHeight() / (float) preview.getHeight());
            const auto dest = juce::Rectangle<float> (preview.getWidth() * k, preview.getHeight() * k).withCentre (box.getCentre());
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (preview, dest);
            g.setColour (colours::line);
            g.drawRect (dest, 1.0f);
        }
    }

    auto label = [&] (juce::Rectangle<int> row, const juce::String& text)
    {
        g.setColour (colours::textDim);
        g.setFont (sans (12.0f, Weight::medium));
        g.drawText (text, row.withWidth (labelW - 10).withHeight (rowH), juce::Justification::centredLeft, true);
    };
    label (shapeRow, tr ("share.shape"));
    label (spanRow, tr ("share.span"));
    label (pitchRow, tr ("share.pitch"));
    label (backRow, tr ("share.background"));

    // 区間の長さと、X に載せるときの目安（超えても切らない。範囲で選べることを示す）
    {
        const auto [from, to] = session.shareSpan (spanKeys.getSelected() == 1);
        const auto seconds = (double) (to - from) / juce::jmax (1, s.sampleRate());
        auto line = spanRow.withTrimmedTop (rowH).withTrimmedLeft (labelW).toFloat();
        g.setFont (sans (11.0f));
        g.setColour (colours::textMute);
        const auto len = tr ("share.length", share::clockText (seconds));
        g.drawText (len, line, juce::Justification::centredLeft, true);
        if (seconds > share::xFreeLimitSeconds)
        {
            g.setColour (colours::warn);
            g.drawText (tr ("share.xLimit"), line.withTrimmedLeft (textWidth (sans (11.0f), len) + 16.0f), juce::Justification::centredLeft, true);
        }
    }

    // 背景の画像の名前（なければスキンの色）
    {
        auto line = backRow.withTrimmedTop (rowH).withTrimmedLeft (labelW).toFloat();
        g.setFont (sans (11.0f));
        g.setColour (backgroundError.isNotEmpty() ? colours::bad : colours::textMute);
        g.drawText (backgroundError.isNotEmpty() ? backgroundError
                                                 : (backgroundName.isNotEmpty() ? backgroundName : tr ("share.background.none")),
                    line, juce::Justification::centredLeft, true);
    }

    label (postLabel, tr ("share.post"));
    if (copied)
    {
        g.setColour (colours::signal);
        g.setFont (sans (11.0f));
        g.drawText (tr ("share.post.copied"), postLabel.withTrimmedLeft (labelW).withRight (copyKey.getX() - 12), juce::Justification::centredRight, true);
    }

    // 権利の注意（アプリが可否を決めたようには書かない。DESIGN 9.1）
    {
        g.setColour (colours::textDim);
        g.setFont (sans (11.0f));
        g.drawFittedText (tr ("share.rights"), rightsArea.withTrimmedRight (4), juce::Justification::topLeft, 4, 1.0f);
    }

    // 状態：書き出し中は進み具合、書き終えたら保存先、できないときは理由
    {
        auto area = statusArea.withTrimmedRight ((cancelKey.isVisible() || openKey.isVisible()) ? cancelKey.getWidth() + 12 : 0).toFloat();
        g.setFont (sans (11.5f));
        if (job.running)
        {
            const auto pct = juce::roundToInt (job.progress * 100.0f);
            auto text = area.removeFromLeft (180.0f);
            g.setColour (colours::text);
            g.drawText (tr ("share.progress", pct), text, juce::Justification::centredLeft, true);
            const auto bar = area.withSizeKeepingCentre (area.getWidth() - 8.0f, 8.0f);
            g.setColour (colours::bgDeep);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (colours::signal);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, job.progress)), 4.0f);
        }
        else if (! session.canShareVideo())
        {
            g.setColour (colours::warn);
            g.drawText (tr ("share.nothing"), area, juce::Justification::centredLeft, true);
        }
        else if (job.error.isNotEmpty())
        {
            g.setColour (colours::bad);
            g.drawFittedText (job.error, area.toNearestInt(), juce::Justification::centredLeft, 2, 1.0f);
        }
        else if (job.file.existsAsFile())
        {
            g.setColour (colours::textDim);
            g.drawText (tr ("share.saved", job.file.getFileName()), area, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (colours::textMute);
            g.drawText (tr ("share.saveTo", s.projectFolder.getChildFile ("share").getFullPathName()), area, juce::Justification::centredLeft, true);
        }
    }
}
} // namespace vb
