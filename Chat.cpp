namespace GOTHIC_ENGINE {
    struct ChatLine {
        string text;
        zCOLOR color;
        bool isSystem = true;
    };

    class Chat {
    private:
        bool isShowing = true;
        unsigned int maxLines = 50;
        std::vector<ChatLine> lines;
        SafeQueue<ChatLine> pendingLines;

    public:
        Chat() {}
        ~Chat() {}

        void Render() {
            while (!pendingLines.isEmpty()) {
                ChatLine l = pendingLines.dequeue();
                if (lines.size() >= maxLines) {
                    lines.erase(lines.begin());
                }
                lines.push_back(l);
            }

            if (!IsShowing())
                return;

            zSTRING prevFont = screen->GetFontName();
            zCOLOR prevColor = screen->fontColor;

            screen->SetFont(zSTRING("Font_Old_10_White_Hi.TGA"));

            const int maxVisible = 6;
            const int lineStep   = 150;
            const int chatX      = 50;
            const int systemStartY = 150; // original top position
            const int chatStartY   = 850; // player chat position

            int count    = min((int)lines.size(), maxVisible);
            int startIdx = (int)lines.size() - count;

            int sysY  = systemStartY;
            int chatY = chatStartY;
            for (int i = 0; i < count; i++) {
                int lineIdx = startIdx + i;
                if (lines[lineIdx].text.Length() == 0)
                    continue;

                screen->SetFontColor(lines[lineIdx].color);
                if (lines[lineIdx].isSystem) {
                    screen->Print(chatX, sysY, zSTRING(lines[lineIdx].text));
                    sysY += lineStep;
                } else {
                    screen->Print(chatX, chatY, zSTRING(lines[lineIdx].text));
                    chatY += lineStep;
                }
                screen->SetFontColor(prevColor);
            }

            screen->SetFont(prevFont);
            screen->SetColor(prevColor);
        }

        bool IsShowing() {
            return isShowing;
        }

        void ToggleShowing() {
            isShowing = !isShowing;
        }

        unsigned int GetLines() {
            return (unsigned int)lines.size();
        }

        void AddLine(string text, zCOLOR color = zCOLOR(255, 255, 255, 255), bool isSystem = true) {
            ChatLine line;
            line.text = text;
            line.color = color;
            line.isSystem = isSystem;
            pendingLines.enqueue(line);
        }

        void Clear() {
            lines.clear();
        }
    };
}
