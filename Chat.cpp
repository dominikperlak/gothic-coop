namespace GOTHIC_ENGINE {
    struct ChatLine {
        string text;
        zCOLOR color;
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

            int y = 150;
            const int lineStep = 150;

            for (unsigned int i = 0; i < lines.size(); ++i) {
                if (lines[i].text.Length() == 0)
                    continue;

                screen->SetFontColor(lines[i].color);
                screen->Print(50, y, zSTRING(lines[i].text));
                screen->SetFontColor(prevColor);

                y += lineStep;
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

        void AddLine(string text, zCOLOR color = zCOLOR(255, 255, 255, 255)) {
            ChatLine line;
            line.text = text;
            line.color = color;
            pendingLines.enqueue(line);
        }

        void Clear() {
            lines.clear();
        }
    };
}
