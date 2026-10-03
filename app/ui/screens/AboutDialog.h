#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

namespace vb
{
/** このアプリについて・ライセンス（DESIGN 11.7「manifest にライセンスを書き、アプリの『ライセンス』画面に出す」・19）
    設定の下のキーから開く。バージョン・アプリのライセンス（AGPL-3.0-or-later）・同梱 / 利用しているもの・モデル（入っているか）。
    キー：ソースコード（AGPL：ソースの場所を示す）・ライセンス全文（GitHub の LICENSE）・閉じる */
class AboutDialog : public DialogPanel
{
public:
    AboutDialog();

    static constexpr const char* sourceUrl  = "https://github.com/kajisho5/voicebooth";
    static constexpr const char* licenseUrl = "https://github.com/kajisho5/voicebooth/blob/main/LICENSE";

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;
};
} // namespace vb
