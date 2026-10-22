#pragma once

#include <QString>
#include <QVector>
#include <cstdint>
#include <tuple>

namespace EditorUi
{
struct AudioEnvironmentRow
{
    uint64_t InstanceId = 0;
    QString Name, Origin, Status, Scope, Icon, PauseHint;
    bool bPaused = false;
    bool bCanPause = false;
    bool bCanStop = false;

    bool operator==(const AudioEnvironmentRow& Other) const
    {
        return std::tie(InstanceId, Name, Origin, Status, Scope, Icon, PauseHint, bPaused, bCanPause, bCanStop) ==
               std::tie(Other.InstanceId, Other.Name, Other.Origin, Other.Status, Other.Scope, Other.Icon,
                        Other.PauseHint, Other.bPaused, Other.bCanPause, Other.bCanStop);
    }
};

struct AudioEnvironmentModel
{
    QString Notice, Footer;
    QVector<AudioEnvironmentRow> Rows;
    bool bCanOpenStory = false;

    bool operator==(const AudioEnvironmentModel& Other) const
    {
        return Notice == Other.Notice && Footer == Other.Footer && Rows == Other.Rows &&
               bCanOpenStory == Other.bCanOpenStory;
    }
};
}
