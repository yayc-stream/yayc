pragma Singleton
import QtQuick
import QtWebEngine
import yayc 1.0

QtObject{
    id: internals

    function getPlayer(isShorts) {
        let res = ""
        if (isShorts) {
//                res += "var ytplayer = document.getElementById('player').getPlayer();
            res += "
var activeShort = document.querySelector('ytd-reel-video-renderer');
var ytplayer = activeShort.querySelector('ytd-player[id=\"player\"]').getPlayer();
"
        } else {
            res += "var ytplayer = document.querySelector('ytd-player').getPlayer();
"
        }
        return res
    }

    property string script_videoTime: "
        var backend;
        new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.backend;
        });
        setTimeout(function() {  //function puller()
                backend.channelURL = document.getElementById('text').firstChild.href;
                backend.channelName = document.getElementById('text').firstChild.text;
                backend.channelAvatar = document.getElementById('owner').firstElementChild.firstElementChild.firstElementChild.firstElementChild.src;

                var ytplayer = document.querySelector('ytd-player').getPlayer();

                backend.videoTitle = ytplayer.getVideoData().title;
                backend.videoDuration = ytplayer.getDuration();
                backend.videoPosition = ytplayer.getCurrentTime();
                backend.playbackRate = ytplayer.getPlaybackRate();
                backend.playerState = ytplayer.getPlayerState();
                backend.volume = ytplayer.getVolume();
                backend.muted = ytplayer.isMuted();
                backend.videoQuality = ytplayer.getPlaybackQuality();
                backend.availableQualityLevels = ytplayer.getAvailableQualityLevels();

                var url = document.getElementsByTagName('ytd-watch-flexy')[0].getAttribute('video-id')
                backend.videoID = url;
                backend.shorts = false;
                backend.vendor = 'YTB';
        }, 100);
        //puller();
    "

    property string script_backend: "
        var backend;
        new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.backend;
        });
    "

    property string script_homePageStatusFetcher: "
        var backend;
        new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.backend;
        });
        setTimeout(function() {
            var btn = document.querySelectorAll(
                'button[id=\"button\"][class=\"style-scope yt-icon-button\"][aria-label=\"Guide\"]')[0]

            backend.guideButtonChecked = btn.getAttribute(\"aria-pressed\")
        }, 100);
    "

    property string script_clickGuide: "
        var backend;
        new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.backend;
        });
        setTimeout(function() {
            var btn = document.querySelectorAll(
                'button[id=\"button\"][class=\"style-scope yt-icon-button\"][aria-label=\"Guide\"]')[0]
            btn.click()
            backend.guideButtonChecked = btn.getAttribute(\"aria-pressed\")
        }, 100);
"

    property string script_videoTimeShorts: "
        var backend;
        new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.backend;
        });
        setTimeout(function() {
                let activeShort = document.querySelector('ytd-reel-video-renderer');
                let ytplayer = activeShort.querySelector('ytd-player[id=\"player\"]').getPlayer();
                let videoData = ytplayer.getVideoData();

                backend.videoID = videoData.video_id;
                backend.shorts = true;
                backend.vendor = 'YTB';

                // Use ytInitialPlayerResponse for static metadata when video ID matches
                let ipr = (typeof ytInitialPlayerResponse !== 'undefined') ? ytInitialPlayerResponse : null;
                let iprMatch = ipr && ipr.videoDetails && ipr.videoDetails.videoId === videoData.video_id;

                if (iprMatch) {
                    backend.videoTitle = ipr.videoDetails.title;
                    backend.channelName = ipr.videoDetails.author;
                    let mf = ipr.microformat && ipr.microformat.playerMicroformatRenderer;
                    backend.channelURL = mf ? mf.ownerProfileUrl : '';
                } else {
                    // Fallback to DOM when ytInitialPlayerResponse is stale (e.g. after swiping)
                    backend.videoTitle = videoData.title || document.title;
                    backend.channelURL = activeShort.getElementsByClassName('yt-core-attributed-string__link yt-core-attributed-string__link--call-to-action-color yt-core-attributed-string--link-inherit-color')[0].href.replace('/shorts', '');
                    backend.channelName = activeShort.getElementsByClassName('yt-core-attributed-string__link yt-core-attributed-string__link--call-to-action-color yt-core-attributed-string--link-inherit-color')[0].textContent;
                }

                // Avatar not available in ytInitialPlayerResponse, keep DOM
                backend.channelAvatar = activeShort.querySelector('.yt-spec-avatar-shape__image.ytCoreImageHost').src;

                // Live playback state from player API
                backend.videoDuration = ytplayer.getDuration();
                backend.videoPosition = ytplayer.getCurrentTime();
                backend.playbackRate = ytplayer.getPlaybackRate();
                backend.playerState = ytplayer.getPlayerState();
                backend.volume = ytplayer.getVolume();
                backend.muted = ytplayer.isMuted();
                backend.videoQuality = ytplayer.getPlaybackQuality();
                backend.availableQualityLevels = ytplayer.getAvailableQualityLevels();
        }, 100);
    "

    function getPlaybackRateSetterScript(rate, isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.setPlaybackRate(" + rate + ");
    }, 100);
"
        return res;
    }

    function getVolumeSetterScript(volume, isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.setVolume(" + volume + ");
    }, 100);
"
        return res;
    }

    function getMutedSetterScript(muted, isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts)

        if (muted) {
            res += "                 ytplayer.mute();
"
        } else {
            res += "                 ytplayer.unMute();
"
        }

        res +=
"       }, 100);
"
        return res;
    }

    readonly property var videoSpeeds: [
        "0.25",
        "0.50",
        "0.75",
        "1.00",
        "1.25",
        "1.50",
        "1.75",
        "2.00"
    ]

    function getPlayVideoScript(isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.playVideo();
    }, 100);
"
        return res;
    }

    function getPlayNextVideoScript(isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.playNextVideo();
    }, 100);
"
        return res;
    }

    function getPauseVideoScript(isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.pauseVideo();
    }, 100);
"
        return res;
    }

    function getQualitySetterScript(quality, isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.setPlaybackQualityRange('" + quality + "', '" + quality + "');
    }, 100);
"
        return res;
    }

    function formatQualityLabel(quality, pad) {
        var labels = {
            "highres": "4320p",
            "hd2880": "2880p",
            "hd2160": "2160p",
            "hd1440": "1440p",
            "hd1080": "1080p",
            "hd720": "720p",
            "large": "480p",
            "medium": "360p",
            "small": "240p",
            "tiny": "144p",
            "auto": "Auto"
        }
        var label = labels[quality] || quality
        if (pad) {
            // Pad to 5 chars (length of "1080p") using non-breaking spaces
            while (label.length < 5) {
                label = "\u00A0" + label + "\u00A0"
            }
        }
        return label
    }
}
