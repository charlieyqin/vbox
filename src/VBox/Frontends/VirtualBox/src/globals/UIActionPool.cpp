/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPool class implementation.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QHelpEvent>
#include <QToolTip>

/* GUI includes: */
#include "UIActionPool.h"
#include "UIActionPoolSelector.h"
#include "UIActionPoolRuntime.h"
#include "UIShortcutPool.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UIExtraDataManager.h"
# include "UINetworkManager.h"
# include "UIUpdateManager.h"
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */


/** QEvent extension
  * representing action-activation event. */
class ActivateActionEvent : public QEvent
{
public:

    /** Constructor. */
    ActivateActionEvent(QAction *pAction)
        : QEvent((QEvent::Type)ActivateActionEventType)
        , m_pAction(pAction) {}

    /** Returns the action this event corresponding to. */
    QAction* action() const { return m_pAction; }

private:

    /** Ho0lds the action this event corresponding to. */
    QAction *m_pAction;
};


UIMenu::UIMenu()
    : m_fShowToolTip(false)
#ifdef Q_WS_MAC
    , m_fConsumable(false)
    , m_fConsumed(false)
#endif /* Q_WS_MAC */
{
}

bool UIMenu::event(QEvent *pEvent)
{
    /* Handle particular event-types: */
    switch (pEvent->type())
    {
        /* Tool-tip request handler: */
        case QEvent::ToolTip:
        {
            /* Get current help-event: */
            QHelpEvent *pHelpEvent = static_cast<QHelpEvent*>(pEvent);
            /* Get action which caused help-event: */
            QAction *pAction = actionAt(pHelpEvent->pos());
            /* If action present => show action's tool-tip if needed: */
            if (pAction && m_fShowToolTip)
                QToolTip::showText(pHelpEvent->globalPos(), pAction->toolTip());
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QMenu::event(pEvent);
}


UIAction::UIAction(UIActionPool *pParent, UIActionType type)
    : QAction(pParent)
    , m_type(type)
    , m_pActionPool(pParent)
    , m_actionPoolType(pParent->type())
    , m_fShortcutHidden(false)
{
    /* By default there is no specific menu role.
     * It will be set explicitly later. */
    setMenuRole(QAction::NoRole);
}

UIMenu* UIAction::menu() const
{
    return qobject_cast<UIMenu*>(QAction::menu());
}

UIActionPolymorphic* UIAction::toActionPolymorphic()
{
    return qobject_cast<UIActionPolymorphic*>(this);
}

void UIAction::setName(const QString &strName)
{
    /* Remember internal name: */
    m_strName = strName;
    /* Update text according new name: */
    updateText();
}

void UIAction::setShortcut(const QKeySequence &shortcut)
{
    /* Only for selector's action-pool: */
    if (m_actionPoolType == UIActionPoolType_Selector)
    {
        /* If shortcut is visible: */
        if (!m_fShortcutHidden)
            /* Call to base-class: */
            QAction::setShortcut(shortcut);
        /* Remember shortcut: */
        m_shortcut = shortcut;
    }
    /* Update text according new shortcut: */
    updateText();
}

void UIAction::showShortcut()
{
    m_fShortcutHidden = false;
    if (!m_shortcut.isEmpty())
        QAction::setShortcut(m_shortcut);
}

void UIAction::hideShortcut()
{
    m_fShortcutHidden = true;
    if (!shortcut().isEmpty())
        QAction::setShortcut(QKeySequence());
}

QString UIAction::nameInMenu() const
{
    /* Action-name format depends on action-pool type: */
    switch (m_actionPoolType)
    {
        /* Unchanged name for Selector UI: */
        case UIActionPoolType_Selector: return name();
        /* Filtered name for Runtime UI: */
        case UIActionPoolType_Runtime: return VBoxGlobal::removeAccelMark(name());
    }
    /* Nothing by default: */
    return QString();
}

void UIAction::updateText()
{
    /* Action-text format depends on action-pool type: */
    switch (m_actionPoolType)
    {
        /* The same as menu name for Selector UI: */
        case UIActionPoolType_Selector:
            setText(nameInMenu());
            break;
        /* With shortcut appended for Runtime UI: */
        case UIActionPoolType_Runtime:
            setText(vboxGlobal().insertKeyToActionText(nameInMenu(),
                                                       gShortcutPool->shortcut(actionPool(), this).toString()));
            break;
    }
}


UIActionMenu::UIActionMenu(UIActionPool *pParent,
                           const QString &strIcon, const QString &strIconDis)
    : UIAction(pParent, UIActionType_Menu)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDis));
    prepare();
}

UIActionMenu::UIActionMenu(UIActionPool *pParent,
                           const QIcon &icon)
    : UIAction(pParent, UIActionType_Menu)
{
    if (!icon.isNull())
        setIcon(icon);
    prepare();
}

void UIActionMenu::setShowToolTip(bool fShowToolTip)
{
    qobject_cast<UIMenu*>(menu())->setShowToolTip(fShowToolTip);
}

void UIActionMenu::prepare()
{
    /* Create menu: */
    setMenu(new UIMenu);
    AssertPtrReturnVoid(menu());
    {
        /* Prepare menu: */
        connect(menu(), SIGNAL(aboutToShow()),
                parent(), SLOT(sltHandleMenuPrepare()));
    }
}

void UIActionMenu::updateText()
{
    setText(nameInMenu());
}


UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QString &strIcon /* = QString() */, const QString &strIconDisabled /* = QString() */)
    : UIAction(pParent, UIActionType_Simple)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
}

UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QString &strIconNormal, const QString &strIconSmall,
                               const QString &strIconNormalDisabled, const QString &strIconSmallDisabled)
    : UIAction(pParent, UIActionType_Simple)
{
    setIcon(UIIconPool::iconSetFull(strIconNormal, strIconSmall, strIconNormalDisabled, strIconSmallDisabled));
}

UIActionSimple::UIActionSimple(UIActionPool *pParent,
                               const QIcon& icon)
    : UIAction(pParent, UIActionType_Simple)
{
    setIcon(icon);
}


UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QString &strIcon /* = QString() */, const QString &strIconDisabled /* = QString() */)
    : UIAction(pParent, UIActionType_Toggle)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
    prepare();
}

UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QString &strIconOn, const QString &strIconOff,
                               const QString &strIconOnDisabled, const QString &strIconOffDisabled)
    : UIAction(pParent, UIActionType_Toggle)
{
    setIcon(UIIconPool::iconSetOnOff(strIconOn, strIconOff, strIconOnDisabled, strIconOffDisabled));
    prepare();
}

UIActionToggle::UIActionToggle(UIActionPool *pParent,
                               const QIcon &icon)
    : UIAction(pParent, UIActionType_Toggle)
{
    if (!icon.isNull())
        setIcon(icon);
    prepare();
}

void UIActionToggle::prepare()
{
    setCheckable(true);
}


UIActionPolymorphic::UIActionPolymorphic(UIActionPool *pParent,
                                         const QString &strIcon /* = QString() */, const QString &strIconDisabled /* = QString() */)
    : UIAction(pParent, UIActionType_Polymorphic)
    , m_iState(0)
{
    if (!strIcon.isNull())
        setIcon(UIIconPool::iconSet(strIcon, strIconDisabled));
}

UIActionPolymorphic::UIActionPolymorphic(UIActionPool *pParent,
                                         const QString &strIconNormal, const QString &strIconSmall,
                                         const QString &strIconNormalDisabled, const QString &strIconSmallDisabled)
    : UIAction(pParent, UIActionType_Polymorphic)
    , m_iState(0)
{
    setIcon(UIIconPool::iconSetFull(strIconNormal, strIconSmall, strIconNormalDisabled, strIconSmallDisabled));
}

UIActionPolymorphic::UIActionPolymorphic(UIActionPool *pParent,
                                         const QIcon& icon)
    : UIAction(pParent, UIActionType_Polymorphic)
    , m_iState(0)
{
    if (!icon.isNull())
        setIcon(icon);
}


class UIActionSimplePreferences : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePreferences(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/global_settings_16px.png")
    {
        setMenuRole(QAction::PreferencesRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const
    {
#ifdef Q_WS_MAC
        return UIExtraDataMetaDefs::MenuApplicationActionType_Preferences;
#else /* !Q_WS_MAC */
        return UIExtraDataMetaDefs::MenuHelpActionType_Preferences;
#endif /* !Q_WS_MAC */
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const
    {
#ifdef Q_WS_MAC
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_Preferences);
#else /* !Q_WS_MAC */
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_Preferences);
#endif /* !Q_WS_MAC */
    }

    QString shortcutExtraDataID() const
    {
        return QString("Preferences");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        switch (actionPool()->type())
        {
            case UIActionPoolType_Selector: return QKeySequence("Ctrl+G");
            case UIActionPoolType_Runtime: break;
        }
        return QKeySequence();
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Preferences...", "global settings"));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global settings window"));
    }
};

class UIActionSimpleLogDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleLogDialog(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png",
                         ":/vm_show_logs_disabled_32px.png", ":/vm_show_logs_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const
    {
#ifdef VBOX_WITH_DEBUGGER_GUI
        return UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_LogDialog;
#else /* !VBOX_WITH_DEBUGGER_GUI */
        return 0;
#endif /* !VBOX_WITH_DEBUGGER_GUI */
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const
    {
#ifdef VBOX_WITH_DEBUGGER_GUI
        return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_LogDialog);
#else /* !VBOX_WITH_DEBUGGER_GUI */
        return QString();
#endif /* !VBOX_WITH_DEBUGGER_GUI */
    }

    QString shortcutExtraDataID() const
    {
        return QString("ShowVMLog");
    }

    QKeySequence defaultShortcut(UIActionPoolType actionPoolType) const
    {
        switch (actionPoolType)
        {
            case UIActionPoolType_Selector: return QKeySequence("Ctrl+L");
            case UIActionPoolType_Runtime: break;
        }
        return QKeySequence();
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Show &Log..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show the log files of the selected virtual machine"));
    }
};

#ifdef RT_OS_DARWIN
class UIActionMenuApplication : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuApplication(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        menu()->setConsumable(true);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_Application; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_Application); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&VirtualBox"));
    }
};

class UIActionSimplePerformClose : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformClose(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuApplicationActionType_Close; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_Close); }

    QString shortcutExtraDataID() const
    {
        return QString("Close");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("Q");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Close..."));
        setStatusTip(QApplication::translate("UIActionPool", "Close the virtual machine"));
    }
};
#endif /* RT_OS_DARWIN */

class UIActionMenuHelp : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuHelp(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_Help; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_Help); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Help"));
    }
};

class UIActionSimpleContents : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleContents(UIActionPool *pParent)
        : UIActionSimple(pParent, UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_DialogHelp))
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuHelpActionType_Contents; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_Contents); }

    QString shortcutExtraDataID() const
    {
        return QString("Help");
    }

    QKeySequence defaultShortcut(UIActionPoolType actionPoolType) const
    {
        switch (actionPoolType)
        {
            case UIActionPoolType_Selector: return QKeySequence(QKeySequence::HelpContents);
            case UIActionPoolType_Runtime: break;
        }
        return QKeySequence();
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Contents..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show help contents"));
    }
};

class UIActionSimpleWebSite : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleWebSite(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/site_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuHelpActionType_WebSite; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_WebSite); }

    QString shortcutExtraDataID() const
    {
        return QString("Web");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&VirtualBox Web Site..."));
        setStatusTip(QApplication::translate("UIActionPool", "Open the browser and go to the VirtualBox product web site"));
    }
};

class UIActionSimpleResetWarnings : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleResetWarnings(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/reset_warnings_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuHelpActionType_ResetWarnings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_ResetWarnings); }

    QString shortcutExtraDataID() const
    {
        return QString("ResetWarnings");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Reset All Warnings"));
        setStatusTip(QApplication::translate("UIActionPool", "Go back to showing all suppressed warnings and messages"));
    }
};

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
class UIActionSimpleNetworkAccessManager : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleNetworkAccessManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/nw_16px.png", ":/nw_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuHelpActionType_NetworkAccessManager; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_NetworkAccessManager); }

    QString shortcutExtraDataID() const
    {
        return QString("NetworkAccessManager");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Network Operations Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show Network Operations Manager"));
    }
};

class UIActionSimpleCheckForUpdates : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCheckForUpdates(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/refresh_16px.png", ":/refresh_disabled_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::MenuHelpActionType_CheckForUpdates; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_CheckForUpdates); }

    QString shortcutExtraDataID() const
    {
        return QString("Update");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "C&heck for Updates..."));
        setStatusTip(QApplication::translate("UIActionPool", "Check for a new VirtualBox version"));
    }
};
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

class UIActionSimpleAbout : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleAbout(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/about_16px.png")
    {
        setMenuRole(QAction::AboutRole);
        retranslateUi();
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const
    {
#ifdef Q_WS_MAC
        return UIExtraDataMetaDefs::MenuApplicationActionType_About;
#else /* !Q_WS_MAC */
        return UIExtraDataMetaDefs::MenuHelpActionType_About;
#endif /* !Q_WS_MAC */
    }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const
    {
#ifdef Q_WS_MAC
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuApplicationActionType_About);
#else /* !Q_WS_MAC */
        return gpConverter->toInternalString(UIExtraDataMetaDefs::MenuHelpActionType_About);
#endif /* !Q_WS_MAC */
    }

    QString shortcutExtraDataID() const
    {
        return QString("About");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&About VirtualBox..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show a window with product information"));
    }
};


/* static */
UIActionPool* UIActionPool::create(UIActionPoolType type)
{
    UIActionPool *pActionPool = 0;
    switch (type)
    {
        case UIActionPoolType_Selector: pActionPool = new UIActionPoolSelector; break;
        case UIActionPoolType_Runtime: pActionPool = new UIActionPoolRuntime; break;
        default: AssertFailedReturn(0);
    }
    AssertPtrReturn(pActionPool, 0);
    pActionPool->prepare();
    return pActionPool;
}

/* static */
void UIActionPool::destroy(UIActionPool *pActionPool)
{
    AssertPtrReturnVoid(pActionPool);
    pActionPool->cleanup();
    delete pActionPool;
}

/* static */
void UIActionPool::createTemporary(UIActionPoolType type)
{
    UIActionPool *pActionPool = 0;
    switch (type)
    {
        case UIActionPoolType_Selector: pActionPool = new UIActionPoolSelector(true); break;
        case UIActionPoolType_Runtime: pActionPool = new UIActionPoolRuntime(true); break;
        default: AssertFailedReturnVoid();
    }
    AssertPtrReturnVoid(pActionPool);
    pActionPool->prepare();
    pActionPool->cleanup();
    delete pActionPool;
}

UIActionPool::UIActionPool(UIActionPoolType type, bool fTemporary /* = false */)
    : m_type(type)
    , m_fTemporary(fTemporary)
{
}

UIActionPoolRuntime* UIActionPool::toRuntime()
{
    return qobject_cast<UIActionPoolRuntime*>(this);
}

UIActionPoolSelector* UIActionPool::toSelector()
{
    return qobject_cast<UIActionPoolSelector*>(this);
}

bool UIActionPool::isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType type) const
{
    foreach (const UIExtraDataMetaDefs::MenuHelpActionType &restriction, m_restrictedActionsMenuHelp.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuHelp(UIActionRestrictionLevel level, UIExtraDataMetaDefs::MenuHelpActionType restriction)
{
    m_restrictedActionsMenuHelp[level] = restriction;
    m_invalidations << UIActionIndex_Menu_Help;
}

#ifdef Q_WS_MAC
bool UIActionPool::isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType type) const
{
    foreach (const UIExtraDataMetaDefs::MenuApplicationActionType &restriction, m_restrictedActionsMenuApplication.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPool::setRestrictionForMenuApplication(UIActionRestrictionLevel level, UIExtraDataMetaDefs::MenuApplicationActionType restriction)
{
    m_restrictedActionsMenuApplication[level] = restriction;
    m_invalidations << UIActionIndex_M_Application;
}
#endif /* Q_WS_MAC */

void UIActionPool::sltHandleMenuPrepare()
{
    /* Make sure menu is valid: */
    UIMenu *pMenu = qobject_cast<UIMenu*>(sender());
    AssertPtrReturnVoid(pMenu);
    /* Make sure action is valid: */
    UIAction *pAction = qobject_cast<UIAction*>(pMenu->menuAction());
    AssertPtrReturnVoid(pAction);

    /* Determine action index: */
    const int iIndex = m_pool.key(pAction);

    /* Update menu if necessary: */
    updateMenu(iIndex);

    /* Notify listeners about menu prepared: */
    emit sigNotifyAboutMenuPrepare(iIndex, pMenu);
}

void UIActionPool::prepare()
{
    /* Prepare pool: */
    preparePool();
    /* Prepare connections: */
    prepareConnections();
    /* Update configuration: */
    updateConfiguration();
    /* Update shortcuts: */
    updateShortcuts();
}

void UIActionPool::preparePool()
{
    /* Create various actions: */
    m_pool[UIActionIndex_Simple_LogDialog] = new UIActionSimpleLogDialog(this);

#ifdef RT_OS_DARWIN
    /* Create 'Application' actions: */
    m_pool[UIActionIndex_M_Application] = new UIActionMenuApplication(this);
    m_pool[UIActionIndex_M_Application_S_About] = new UIActionSimpleAbout(this);
    m_pool[UIActionIndex_M_Application_S_Preferences] = new UIActionSimplePreferences(this);
    m_pool[UIActionIndex_M_Application_S_Close] = new UIActionSimplePerformClose(this);
#endif /* RT_OS_DARWIN */

    /* Create 'Help' actions: */
    m_pool[UIActionIndex_Menu_Help] = new UIActionMenuHelp(this);
    m_pool[UIActionIndex_Simple_Contents] = new UIActionSimpleContents(this);
    m_pool[UIActionIndex_Simple_WebSite] = new UIActionSimpleWebSite(this);
    m_pool[UIActionIndex_Simple_ResetWarnings] = new UIActionSimpleResetWarnings(this);
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    m_pool[UIActionIndex_Simple_NetworkAccessManager] = new UIActionSimpleNetworkAccessManager(this);
    m_pool[UIActionIndex_Simple_CheckForUpdates] = new UIActionSimpleCheckForUpdates(this);
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef RT_OS_DARWIN
    m_pool[UIActionIndex_Simple_About] = new UIActionSimpleAbout(this);
    m_pool[UIActionIndex_Simple_Preferences] = new UIActionSimplePreferences(this);
#endif /* !RT_OS_DARWIN */

    /* Prepare update-handlers for known menus: */
#ifdef RT_OS_DARWIN
    m_menuUpdateHandlers[UIActionIndex_M_Application].ptf = &UIActionPool::updateMenuApplication;
#endif /* RT_OS_DARWIN */
    m_menuUpdateHandlers[UIActionIndex_Menu_Help].ptf = &UIActionPool::updateMenuHelp;

    /* Invalidate all known menus: */
    m_invalidations.unite(m_menuUpdateHandlers.keys().toSet());

    /* Retranslate finally: */
    retranslateUi();
}

void UIActionPool::cleanupPool()
{
    /* Cleanup pool: */
    qDeleteAll(m_pool);
}

void UIActionPool::cleanup()
{
    /* Cleanup pool: */
    cleanupPool();
}

void UIActionPool::updateShortcuts()
{
    gShortcutPool->applyShortcuts(this);
}

bool UIActionPool::processHotKey(const QKeySequence &key)
{
    /* Iterate through the whole list of keys: */
    foreach (const int &iKey, m_pool.keys())
    {
        /* Get current action: */
        UIAction *pAction = m_pool.value(iKey);
        /* Skip menus/separators: */
        if (pAction->type() == UIActionType_Menu)
            continue;
        /* Get the hot-key of the current action: */
        const QString strHotKey = gShortcutPool->shortcut(this, pAction).toString();
        if (pAction->isEnabled() && pAction->isVisible() && !strHotKey.isEmpty())
        {
            if (key.matches(QKeySequence(strHotKey)) == QKeySequence::ExactMatch)
            {
                /* We asynchronously post a special event instead of calling
                 * pAction->trigger() directly, to let key presses and
                 * releases be processed correctly by Qt first.
                 * Note: we assume that nobody will delete the menu item
                 * corresponding to the key sequence, so that the pointer to
                 * menu data posted along with the event will remain valid in
                 * the event handler, at least until the main window is closed. */
                QApplication::postEvent(this, new ActivateActionEvent(pAction));
                return true;
            }
        }
    }
    return false;
}

void UIActionPool::updateConfiguration()
{
    /* Recache common action restrictions: */
    // Nothing here for now..

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Recache update action restrictions: */
    bool fUpdateAllowed = gEDataManager->applicationUpdateEnabled();
    if (!fUpdateAllowed)
    {
        m_restrictedActionsMenuHelp[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::MenuHelpActionType)
            (m_restrictedActionsMenuHelp[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::MenuHelpActionType_CheckForUpdates);
    }
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Update menus: */
    updateMenus();
}

void UIActionPool::updateMenu(int iIndex)
{
    /* Update if menu with such index is invalidated and there is update-handler: */
    if (m_invalidations.contains(iIndex) && m_menuUpdateHandlers.contains(iIndex))
        (this->*(m_menuUpdateHandlers.value(iIndex).ptf))();
}

#ifdef RT_OS_DARWIN
void UIActionPool::updateMenuApplication()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndex_M_Application)->menu();
    AssertReturnVoid(pMenu && pMenu->isConsumable());
    /* Clear contents: */
    if (!pMenu->isConsumed())
        pMenu->clear();

    /* 'About' action: */
    const bool fAllowToShowActionAbout = isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_About);
    action(UIActionIndex_M_Application_S_About)->setEnabled(fAllowToShowActionAbout);
    action(UIActionIndex_M_Application_S_About)->setVisible(fAllowToShowActionAbout);
    if (!pMenu->isConsumed())
    {
        pMenu->addAction(action(UIActionIndex_M_Application_S_About));
        connect(action(UIActionIndex_M_Application_S_About), SIGNAL(triggered()),
                &msgCenter(), SLOT(sltShowHelpAboutDialog()), Qt::UniqueConnection);
    }

    /* Only for Runtime pool: */
    if (type() == UIActionPoolType_Runtime)
    {
        /* 'Preferences' action: */
        const bool fAllowToShowActionPreferences = isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_Preferences);
        action(UIActionIndex_M_Application_S_Preferences)->setEnabled(fAllowToShowActionPreferences);
        action(UIActionIndex_M_Application_S_Preferences)->setVisible(fAllowToShowActionPreferences);
        if (!pMenu->isConsumed())
            pMenu->addAction(action(UIActionIndex_M_Application_S_Preferences));
    }

    /* Close action: */
    const bool fAllowToShowActionClose = isAllowedInMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_Close);
    action(UIActionIndex_M_Application_S_Close)->setEnabled(fAllowToShowActionClose);
    if (!pMenu->isConsumed())
        pMenu->addAction(action(UIActionIndex_M_Application_S_Close));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_M_Application);
}
#endif /* RT_OS_DARWIN */

void UIActionPool::updateMenuHelp()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndex_Menu_Help)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();


    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Contents' action: */
    const bool fAllowToShowActionContents = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Contents);
    action(UIActionIndex_Simple_Contents)->setEnabled(fAllowToShowActionContents);
    if (fAllowToShowActionContents)
    {
        pMenu->addAction(action(UIActionIndex_Simple_Contents));
        connect(action(UIActionIndex_Simple_Contents), SIGNAL(triggered()),
                &msgCenter(), SLOT(sltShowHelpHelpDialog()), Qt::UniqueConnection);
        fSeparator1 = true;
    }

    /* 'Web Site' action: */
    const bool fAllowToShowActionWebSite = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_WebSite);
    action(UIActionIndex_Simple_WebSite)->setEnabled(fAllowToShowActionWebSite);
    if (fAllowToShowActionWebSite)
    {
        pMenu->addAction(action(UIActionIndex_Simple_WebSite));
        connect(action(UIActionIndex_Simple_WebSite), SIGNAL(triggered()),
                &msgCenter(), SLOT(sltShowHelpWebDialog()), Qt::UniqueConnection);
        fSeparator1 = true;
    }

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* 'Reset Warnings' action: */
    const bool fAllowToShowActionResetWarnings = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_ResetWarnings);
    action(UIActionIndex_Simple_ResetWarnings)->setEnabled(fAllowToShowActionResetWarnings);
    if (fAllowToShowActionResetWarnings)
    {
        pMenu->addAction(action(UIActionIndex_Simple_ResetWarnings));
        connect(action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                &msgCenter(), SLOT(sltResetSuppressedMessages()), Qt::UniqueConnection);
        fSeparator2 = true;
    }

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# ifndef Q_WS_MAC
    /* Separator #3? */
    bool fSeparator3 = false;
# endif /* !Q_WS_MAC */

    /* 'Network Manager' action: */
    const bool fAllowToShowActionNetworkManager = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_NetworkAccessManager);
    action(UIActionIndex_Simple_NetworkAccessManager)->setEnabled(fAllowToShowActionNetworkManager);
    if (fAllowToShowActionNetworkManager)
    {
        pMenu->addAction(action(UIActionIndex_Simple_NetworkAccessManager));
        connect(action(UIActionIndex_Simple_NetworkAccessManager), SIGNAL(triggered()),
                gNetworkManager, SLOT(show()), Qt::UniqueConnection);
# ifndef Q_WS_MAC
        fSeparator3 = true;
# endif /* !Q_WS_MAC */
    }

    /* Only for Selector pool: */
    if (type() == UIActionPoolType_Selector)
    {
        /* 'Check for Updates' action: */
        const bool fAllowToShowActionCheckForUpdates = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_CheckForUpdates);
        action(UIActionIndex_Simple_NetworkAccessManager)->setEnabled(fAllowToShowActionCheckForUpdates);
        if (fAllowToShowActionCheckForUpdates)
        {
            pMenu->addAction(action(UIActionIndex_Simple_CheckForUpdates));
            connect(action(UIActionIndex_Simple_CheckForUpdates), SIGNAL(triggered()),
                    gUpdateManager, SLOT(sltForceCheck()), Qt::UniqueConnection);
# ifndef Q_WS_MAC
            fSeparator3 = true;
# endif /* !Q_WS_MAC */
        }
    }

# ifndef Q_WS_MAC
    /* Separator #3: */
    if (fSeparator3)
        pMenu->addSeparator();
# endif /* !Q_WS_MAC */
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */


#ifndef RT_OS_DARWIN
    /* 'About' action: */
    const bool fAllowToShowActionAbout = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_About);
    action(UIActionIndex_Simple_About)->setEnabled(fAllowToShowActionAbout);
    if (fAllowToShowActionAbout)
    {
        pMenu->addAction(action(UIActionIndex_Simple_About));
        connect(action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                &msgCenter(), SLOT(sltShowHelpAboutDialog()), Qt::UniqueConnection);
    }

    /* Only for Runtime pool: */
    if (type() == UIActionPoolType_Runtime)
    {
        /* 'Preferences' action: */
        const bool fAllowToShowActionPreferences = isAllowedInMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Preferences);
        action(UIActionIndex_Simple_Preferences)->setEnabled(fAllowToShowActionPreferences);
        if (fAllowToShowActionPreferences)
            pMenu->addAction(action(UIActionIndex_Simple_Preferences));
    }
#endif /* !RT_OS_DARWIN */


    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndex_Menu_Help);
}

void UIActionPool::retranslateUi()
{
    /* Translate all the actions: */
    foreach (const int iActionPoolKey, m_pool.keys())
        m_pool[iActionPoolKey]->retranslateUi();
    /* Update shortcuts: */
    updateShortcuts();
}

bool UIActionPool::event(QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        case ActivateActionEventType:
        {
            /* Process specific event: */
            ActivateActionEvent *pActionEvent = static_cast<ActivateActionEvent*>(pEvent);
            pActionEvent->action()->trigger();
            pEvent->accept();
            return true;
        }
        default:
            break;
    }
    /* Pass to the base-class: */
    return QObject::event(pEvent);
}

#include "UIActionPool.moc"

