/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

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
#include "../jucer_Headers.h"
#include "../Application/jucer_Application.h"
#include "../Project Saving/jucer_ProjectExporter.h"
#include "../Project/jucer_HeaderComponent.h"
#include "jucer_LicenseController.h"

#include "jucer_LicenseWebview.h"
#include "jucer_LicenseThread.h"

//==============================================================================
const char* LicenseState::licenseTypeToString (LicenseState::Type type)
{
    switch (type)
    {
        case Type::notLoggedIn:         return "<notLoggedIn>";
        case Type::noLicenseChosenYet:  return "<noLicenseChosenYet>";
        case Type::GPL:                 return "JUCE GPL";
        case Type::personal:            return "JUCE Personal";
        case Type::edu:                 return "JUCE Education";
        case Type::indie:               return "JUCE Indie";
        case Type::pro:                 return "JUCE Pro";
        default:                        return "<unknown>";
    }
}

static const char* getLicenseStateValue (LicenseState::Type type)
{
    switch (type)
    {
        case LicenseState::Type::GPL:       return "GPL";
        case LicenseState::Type::personal:  return "personal";
        case LicenseState::Type::edu:       return "edu";
        case LicenseState::Type::indie:     return "indie";
        case LicenseState::Type::pro:       return "pro";
        default:                            return nullptr;
    }
}

static LicenseState::Type getLicenseTypeFromValue (const String& d)
{
    if (d == getLicenseStateValue (LicenseState::Type::GPL))       return LicenseState::Type::GPL;
    if (d == getLicenseStateValue (LicenseState::Type::personal))  return LicenseState::Type::personal;
    if (d == getLicenseStateValue (LicenseState::Type::edu))       return LicenseState::Type::edu;
    if (d == getLicenseStateValue (LicenseState::Type::indie))     return LicenseState::Type::indie;
    if (d == getLicenseStateValue (LicenseState::Type::pro))       return LicenseState::Type::pro;
    return LicenseState::Type::noLicenseChosenYet;
}

//==============================================================================
struct LicenseController::ModalCompletionCallback : ModalComponentManager::Callback
{
    ModalCompletionCallback (LicenseController& controller) : owner (controller) {}
    void modalStateFinished (int returnValue) override       { owner.modalStateFinished (returnValue); }
    LicenseController& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalCompletionCallback)
};

//==============================================================================
LicenseController::LicenseController()
   #if (! JUCER_ENABLE_GPL_MODE)
    : state (licenseStateFromSettings (ProjucerApplication::getApp().settings->getGlobalProperties()))
   #endif
{
   #if JUCER_ENABLE_GPL_MODE
    state.type     = LicenseState::Type::GPL;
    state.username = "GPL mode";
   #else
    thread = new LicenseThread (*this, false);
   #endif
}

LicenseController::~LicenseController()
{
    thread = nullptr;
    closeWebview (-1);
}

void LicenseController::logout()
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

   #if ! JUCER_ENABLE_GPL_MODE
    thread = nullptr;
    updateState ({});

   #if ! JUCE_LINUX
    WebBrowserComponent::clearCookies();
   #endif

    thread = new LicenseThread (*this, false);
   #endif
}

void LicenseController::chooseNewLicense()
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

   #if ! JUCER_ENABLE_GPL_MODE
    thread = nullptr;
    thread = new LicenseThread (*this, true);
   #endif
}

//==============================================================================
void LicenseController::closeWebview (int result)
{
    if (licenseWebview != nullptr)
        licenseWebview->exitModalState (result);
}

void LicenseController::modalStateFinished (int result)
{
    licenseWebview = nullptr;

    if (result == -1 && (state.type == LicenseState::Type::notLoggedIn
                          || state.type == LicenseState::Type::noLicenseChosenYet))
        JUCEApplication::getInstance()->systemRequestedQuit();
}

void LicenseController::ensureLicenseWebviewIsOpenWithPage (const String& param)
{
    if (licenseWebview != nullptr)
    {
        licenseWebview->goToURL (param);
        licenseWebview->toFront (true);
    }
    else
    {
       #if ! JUCE_LINUX
        WebBrowserComponent::clearCookies();
       #endif

        licenseWebview = new LicenseWebview (new ModalCompletionCallback (*this), param);
    }
}

void LicenseController::queryWebview (const String& startURL, const String& valueToQuery,
                                      HashMap<String, String>& result)
{
    ensureLicenseWebviewIsOpenWithPage (startURL);

    licenseWebview->setPageCallback ([this,valueToQuery,&result] (const String& cmd, const HashMap<String, String>& params)
    {
        if (valueToQuery.isEmpty() || cmd == valueToQuery)
        {
            result.clear();

            for (HashMap<String, String>::Iterator it = params.begin(); it != params.end(); ++it)
                result.set (it.getKey(), it.getValue());

            if (thread != nullptr && ! thread->threadShouldExit())
                thread->finished.signal();
        }
    });

    licenseWebview->setNewWindowCallback ([this, &result] (const String& url)
    {
        if (url.endsWith ("get-juce/indie") || url.endsWith ("get-juce/pro"))
        {
            result.clear();
            result.set ("page-redirect", url);

            if (thread != nullptr && ! thread->threadShouldExit())
                thread->finished.signal();
        }
    });
}

void LicenseController::updateState (const LicenseState& newState)
{
    auto& props = ProjucerApplication::getApp().settings->getGlobalProperties();

    state = newState;
    licenseStateToSettings (state, props);
    listeners.call (&StateChangedCallback::licenseStateChanged, state);
}

LicenseState LicenseController::licenseStateFromSettings (PropertiesFile& props)
{
    ScopedPointer<XmlElement> licenseXml = props.getXmlValue ("license");

    if (licenseXml != nullptr)
    {
        LicenseState result;
        result.type      = getLicenseTypeFromValue (licenseXml->getChildElementAllSubText ("type", {}));
        result.username  = licenseXml->getChildElementAllSubText ("username", {});
        result.email     = licenseXml->getChildElementAllSubText ("email", {});
        result.authToken = licenseXml->getChildElementAllSubText ("authToken", {});

        MemoryOutputStream imageData;
        Base64::convertFromBase64 (imageData, licenseXml->getChildElementAllSubText ("avatar", {}));
        result.avatar = ImageFileFormat::loadFrom (imageData.getData(), imageData.getDataSize());

        return result;
    }

    return {};
}

void LicenseController::licenseStateToSettings (const LicenseState& state, PropertiesFile& props)
{
    props.removeValue ("license");

    if (state.type != LicenseState::Type::notLoggedIn
          && state.username.isNotEmpty() && state.authToken.isNotEmpty())
    {
        XmlElement licenseXml ("license");

        if (auto* typeString = getLicenseStateValue (state.type))
            licenseXml.createNewChildElement ("type")->addTextElement (typeString);

        licenseXml.createNewChildElement ("username")->addTextElement (state.username);
        licenseXml.createNewChildElement ("email")   ->addTextElement (state.email);

        // TODO: encrypt authToken
        licenseXml.createNewChildElement ("authToken")->addTextElement (state.authToken);

        MemoryOutputStream imageData;
        if (state.avatar.isValid() && PNGImageFormat().writeImageToStream (state.avatar, imageData))
            licenseXml.createNewChildElement ("avatar")->addTextElement (Base64::toBase64 (imageData.getData(), imageData.getDataSize()));

        props.setValue ("license", &licenseXml);
    }

    props.saveIfNeeded();
}
