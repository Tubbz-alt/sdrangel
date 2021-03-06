///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "gui/featurewindow.h"
#include "plugin/plugininstancegui.h"
#include "plugin/pluginapi.h"
#include "settings/featuresetpreset.h"
#include "feature/featureutils.h"

#include "featureuiset.h"

FeatureUISet::FeatureUISet(int tabIndex)
{
    m_featureWindow = new FeatureWindow;
    m_featureTabIndex = tabIndex;
}

FeatureUISet::~FeatureUISet()
{
    delete m_featureWindow;
}

void FeatureUISet::addRollupWidget(QWidget *widget)
{
    m_featureWindow->addRollupWidget(widget);
}

void FeatureUISet::registerFeatureInstance(const QString& featureName, PluginInstanceGUI* pluginGUI, Feature *feature)
{
    m_featureInstanceRegistrations.append(FeatureInstanceRegistration(featureName, pluginGUI, feature));
    renameFeatureInstances();
}

void FeatureUISet::removeFeatureInstance(PluginInstanceGUI* pluginGUI)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == pluginGUI)
        {
            m_featureInstanceRegistrations.erase(it);
            break;
        }
    }

    renameFeatureInstances();
}

void FeatureUISet::renameFeatureInstances()
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++) {
        m_featureInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_featureInstanceRegistrations[i].m_featureName).arg(i));
    }
}

// sort by name
bool FeatureUISet::FeatureInstanceRegistration::operator<(const FeatureInstanceRegistration& other) const
{
    if (m_gui && other.m_gui) {
        return m_gui->getName() < other.m_gui->getName();
    } else {
        return false;
    }
}

void FeatureUISet::freeFeatures()
{
    for(int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::freeFeatures: destroying feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_featureName));
        m_featureInstanceRegistrations[i].m_gui->destroy();
    }
}

void FeatureUISet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureUISet::deleteFeature: delete feature [%s] at %d",
                qPrintable(m_featureInstanceRegistrations[featureIndex].m_featureName),
                featureIndex);
        m_featureInstanceRegistrations[featureIndex].m_gui->destroy();
        renameFeatureInstances();
    }
}

const Feature *FeatureUISet::getFeatureAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}

Feature *FeatureUISet::getFeatureAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}

void FeatureUISet::loadFeatureSetSettings(const FeatureSetPreset *preset, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter)
{
    qDebug("FeatureUISet::loadFeatureSetSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

    // Available feature plugins
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations();

    // copy currently open features and clear list
    FeatureInstanceRegistrations openFeatures = m_featureInstanceRegistrations;
    m_featureInstanceRegistrations.clear();

    for (int i = 0; i < openFeatures.count(); i++)
    {
        qDebug("FeatureUISet::loadFeatureSetSettings: destroying old feature [%s]", qPrintable(openFeatures[i].m_featureName));
        openFeatures[i].m_gui->destroy();
    }

    qDebug("FeatureUISet::loadFeatureSetSettings: %d feature(s) in preset", preset->getFeatureCount());

    for (int i = 0; i < preset->getFeatureCount(); i++)
    {
        const FeatureSetPreset::FeatureConfig& featureConfig = preset->getFeatureConfig(i);
        FeatureInstanceRegistration reg;

        // create feature instance

        for(int i = 0; i < featureRegistrations->count(); i++)
        {
            if (FeatureUtils::compareFeatureURIs((*featureRegistrations)[i].m_featureIdURI, featureConfig.m_featureIdURI))
            {
                qDebug("FeatureUISet::loadFeatureSetSettings: creating new feature [%s] from config [%s]",
                        qPrintable((*featureRegistrations)[i].m_featureIdURI),
                        qPrintable(featureConfig.m_featureIdURI));
                Feature *feature =
                        (*featureRegistrations)[i].m_plugin->createFeature(apiAdapter);
                PluginInstanceGUI *featureGUI =
                        (*featureRegistrations)[i].m_plugin->createFeatureGUI(this, feature);
                reg = FeatureInstanceRegistration(featureConfig.m_featureIdURI, featureGUI, feature);
                break;
            }
        }

        if (reg.m_gui)
        {
            qDebug("FeatureUISet::loadFeatureSetSettings: deserializing feature [%s]", qPrintable(featureConfig.m_featureIdURI));
            reg.m_gui->deserialize(featureConfig.m_config);
        }
    }

    renameFeatureInstances();
}

void FeatureUISet::saveFeatureSetSettings(FeatureSetPreset *preset)
{
    std::sort(m_featureInstanceRegistrations.begin(), m_featureInstanceRegistrations.end()); // sort by increasing delta frequency and type

    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::saveFeatureSetSettings: saving feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_featureName));
        preset->addFeature(m_featureInstanceRegistrations[i].m_featureName, m_featureInstanceRegistrations[i].m_gui->serialize());
    }
}
