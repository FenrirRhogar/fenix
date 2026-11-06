#pragma once

// Update check shared by the fenrikos plugins (the same file in each repo).
//
// At most once a day, when a plugin window opens, a background thread asks GitHub for the latest
// release of the plugin's repo and compares it with the running version. GitHub sees an IP address
// and a generic user agent; nothing about the user or the session is sent. The result and an on/off
// setting are kept in <user app data>/fenrikos/<name>.settings (~/.config/fenrikos on Linux), shared
// by every instance.
//
// The plugin defines, e.g. in CMake:
//   FENRIKOS_UPDATE_REPO  "fenix"                              GitHub repo under FenrirRhogar
//   FENRIKOS_UPDATE_NAME  "Fenix"                              settings file name
//   FENRIKOS_PRODUCT_URL  "https://fenrikos.gumroad.com/l/fenix"
// and shows an UpdateBadge where it prints its version.
//
// On Linux this needs JUCE_USE_CURL=1 with JUCE_LOAD_CURL_SYMBOLS_LAZILY=1: libcurl is loaded when
// the check runs, and the check quietly does nothing if it is missing.

#include <juce_gui_basics/juce_gui_basics.h>

namespace fenrikos
{

class UpdateCheck : public juce::ChangeBroadcaster,
                    private juce::Thread,
                    private juce::AsyncUpdater
{
public:
    UpdateCheck() : juce::Thread ("fenrikos update check")
    {
        juce::PropertiesFile::Options options;
        options.applicationName = FENRIKOS_UPDATE_NAME;
        options.filenameSuffix = ".settings";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = ".config/fenrikos";   // JUCE puts Linux settings straight in the home folder otherwise
       #else
        options.folderName = "fenrikos";
       #endif
        options.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (options);
        latest = settings->getValue ("latestVersion");
    }

    ~UpdateCheck() override
    {
        // Abort a request still in flight, so closing the plugin window neither waits for the
        // network nor leaves JUCE to kill the thread by force inside libcurl.
        signalThreadShouldExit();
        {
            const juce::ScopedLock sl (lock);
            if (request != nullptr)
                request->cancel();
        }
        stopThread (10000);
        cancelPendingUpdate();
    }

    static juce::String currentVersion() { return JucePlugin_VersionString; }
    static juce::String productUrl()     { return FENRIKOS_PRODUCT_URL; }

    bool isEnabled() const          { return settings->getBoolValue ("checkForUpdates", true); }
    juce::String latestVersion() const { return latest; }
    bool isUpdateAvailable() const  { return isEnabled() && isNewer (latest, currentVersion()); }

    void setEnabled (bool shouldCheck)
    {
        settings->setValue ("checkForUpdates", shouldCheck);
        settings->saveIfNeeded();
        if (shouldCheck)
            checkIfDue();
        sendChangeMessage();
    }

    /// Message thread. Starts a check if it is switched on and the last one is a day old.
    void checkIfDue()
    {
        constexpr juce::int64 day = 24 * 60 * 60 * 1000;
        const auto last = settings->getValue ("lastCheck").getLargeIntValue();
        if (isEnabled() && ! isThreadRunning() && juce::Time::currentTimeMillis() - last > day)
            startThread (juce::Thread::Priority::low);
    }

    /// True when version @p a is newer than @p b ("1.10.0" > "1.9.2"; a leading "v" is ignored).
    static bool isNewer (const juce::String& a, const juce::String& b)
    {
        const auto pa = juce::StringArray::fromTokens (a.trimCharactersAtStart ("vV"), ".", {});
        const auto pb = juce::StringArray::fromTokens (b.trimCharactersAtStart ("vV"), ".", {});
        if (pa.isEmpty() || pa[0].isEmpty())
            return false;
        for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
        {
            const int x = pa[i].getIntValue(), y = pb[i].getIntValue();
            if (x != y)
                return x > y;
        }
        return false;
    }

private:
    void run() override
    {
        const juce::URL url ("https://api.github.com/repos/FenrirRhogar/" FENRIKOS_UPDATE_REPO "/releases/latest");
        juce::WebInputStream stream (url, false);
        stream.withConnectionTimeout (5000)
              .withExtraHeaders ("User-Agent: fenrikos-update-check\r\nAccept: application/vnd.github+json");
        {
            const juce::ScopedLock sl (lock);
            if (threadShouldExit())
                return;
            request = &stream;   // the destructor cancels it from the message thread
        }

        juce::String body;
        if (stream.connect (nullptr) && stream.getStatusCode() == 200)
            body = stream.readEntireStreamAsString();

        const juce::ScopedLock sl (lock);
        request = nullptr;
        const auto tag = juce::JSON::parse (body).getProperty ("tag_name", {}).toString().trimCharactersAtStart ("vV");
        if (threadShouldExit() || tag.isEmpty())
            return;

        fetched = tag;
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        {
            const juce::ScopedLock sl (lock);
            latest = fetched;
        }
        settings->setValue ("latestVersion", latest);
        settings->setValue ("lastCheck", juce::String (juce::Time::currentTimeMillis()));
        settings->saveIfNeeded();
        sendChangeMessage();
    }

    std::unique_ptr<juce::PropertiesFile> settings;
    juce::String latest, fetched;
    juce::WebInputStream* request = nullptr;   // the request in flight, guarded by `lock`
    juce::CriticalSection lock;
};

//==============================================================================
/** The plugin's version text, as a small clickable label. It reads "UPDATE x.y.z AVAILABLE" in
    the highlight colour when there is one; a click opens a menu with the product page and the
    "Check for updates automatically" switch. */
class UpdateBadge : public juce::Component,
                    private juce::ChangeListener
{
public:
    UpdateBadge (juce::String versionText, juce::Font labelFont, juce::Colour normalColour, juce::Colour highlightColour,
                 juce::Justification justification)
        : text (std::move (versionText)), font (std::move (labelFont)), normal (normalColour), highlight (highlightColour),
          just (justification)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        check->addChangeListener (this);
        check->checkIfDue();
    }

    ~UpdateBadge() override { check->removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        const bool update = check->isUpdateAvailable();
        g.setFont (font);
        g.setColour (update ? highlight : (isMouseOver() ? normal.brighter (0.4f) : normal));
        g.drawText (update ? "UPDATE " + check->latestVersion() + " AVAILABLE" : text, getLocalBounds(), just, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void mouseUp (const juce::MouseEvent&) override
    {
        juce::PopupMenu menu;
        if (check->isUpdateAvailable())
            menu.addItem ("Get version " + check->latestVersion() + "...", [] { juce::URL (UpdateCheck::productUrl()).launchInDefaultBrowser(); });
        else
            menu.addItem ("Open the product page...", [] { juce::URL (UpdateCheck::productUrl()).launchInDefaultBrowser(); });
        menu.addSeparator();
        menu.addItem ("Check for updates automatically", true, check->isEnabled(),
                      [c = check] () mutable { c->setEnabled (! c->isEnabled()); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }

    static int preferredWidth (const juce::Font& f)
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (f, "UPDATE 88.88.88 AVAILABLE")) + 8;
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    juce::SharedResourcePointer<UpdateCheck> check;
    juce::String text;
    juce::Font font;
    juce::Colour normal, highlight;
    juce::Justification just;
};

} // namespace fenrikos
