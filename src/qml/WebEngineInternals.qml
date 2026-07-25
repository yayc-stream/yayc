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

    property string script_autoSkipAd: "
        (function() {
            if (window.__yayc_adskip) return;
            window.__yayc_adskip = true;
            console.log('[yayc-adskip] script loaded');
            var backend = null;
            new QWebChannel(qt.webChannelTransport, function(channel) {
                backend = channel.objects.backend;
                console.log('[yayc-adskip] backend connected');
            });
            function trySkip() {
                if (!window.__yayc_autoSkipAdEnabled) return false;
                if (!backend) return false;
                var ad = document.querySelector('.ad-showing');
                if (!ad) return false;
                var btn = document.querySelector('.ytp-skip-ad-button');
                if (!btn || btn.style.display === 'none') return false;
                var rect = btn.getBoundingClientRect();
                if (rect.bottom < 0 || rect.top > window.innerHeight
                    || rect.right < 0 || rect.left > window.innerWidth) {
                    btn.scrollIntoView({block: 'center', behavior: 'instant'});
                    return false;
                }
                var cx = rect.left + rect.width / 2;
                var cy = rect.top + rect.height / 2;
                console.log('[yayc-adskip] requesting skip at ' + cx + ',' + cy);
                backend.skipAdX = cx;
                backend.skipAdY = cy;
                backend.skipAdSeq = (backend.skipAdSeq || 0) + 1;
                return true;
            }
            var obs = new MutationObserver(function() { trySkip(); });
            obs.observe(document.body, { childList: true, subtree: true });
            setInterval(trySkip, 500);
        })();
    "

    // Keeps a "pinned" hover preview alive. The Qt-side event filter already stops
    // hover traffic from reaching Chromium (so no new hover is computed), but that
    // can't cover events Chromium generates internally from geometry - notably on
    // scroll. So while pinned we also blanket-block the leave family in the capture
    // phase, plus resume playback if something pauses the preview anyway.
    //
    // The block is deliberately NOT scoped to the ytd-video-preview subtree: the
    // DevTools trace showed YouTube's dismiss handlers sit on ancestor containers
    // (ytd-rich-item-renderer, yt-lockup-view-model) *outside* the preview element,
    // so a scoped filter would miss them. Collateral effect is that other page
    // hover states can stick while pinned, which is cosmetic and transient.
    property string script_previewPin: "
        (function() {
            if (window.__yayc_pinctl) return;
            window.__yayc_pinctl = true;
            var backend = null;
            new QWebChannel(qt.webChannelTransport, function(channel) {
                backend = channel.objects.backend;
            });

            function block(e) {
                if (window.__yayc_pinned)
                    e.stopImmediatePropagation();
            }
            // Both directions have to go, not just the leave family. On scroll
            // Chromium dispatches a synthetic mousemove to re-resolve what sits under
            // the last known pointer position, then fires over/enter at whatever
            // element just arrived there - which made YouTube start *that* thumbnail's
            // preview as soon as one scrolled under the frozen point. These are
            // generated inside Chromium, so the Qt-side filter cannot see them; only a
            // DOM-level block reaches them.
            var types = ['mouseleave', 'mouseout', 'pointerleave', 'pointerout',
                         'mouseover', 'mouseenter', 'pointerover', 'pointerenter',
                         'mousemove', 'pointermove'];
            for (var i = 0; i < types.length; ++i)
                document.addEventListener(types[i], block, true);

            // Backstop: whatever route YouTube took to pause it, resume.
            document.addEventListener('pause', function(e) {
                if (!window.__yayc_pinned) return;
                var v = e.target;
                if (!v || v.tagName !== 'VIDEO') return;
                if (!v.closest || !v.closest('ytd-video-preview')) return;
                try { v.play(); } catch (err) {}
            }, true);

            // Unmuting is safe here, unlike in the general applier: a pin always
            // follows a right-click, so Chromium's user-activation requirement for
            // audible playback is satisfied and it won't pause us instead.
            //
            // Note this can only help if the preview actually carries audio. Some
            // previews (music videos especially) appear to be served without an
            // audio stream - that is why YouTube shows no unmute affordance on them -
            // and for those there is nothing to unmute.
            window.__yayc_unmutePreviews = function() {
                var hosts = document.querySelectorAll('ytd-video-preview ytd-player');
                for (var i = 0; i < hosts.length; ++i) {
                    var yt = null;
                    try { yt = hosts[i].getPlayer ? hosts[i].getPlayer() : null; } catch (e) {}
                    if (yt && yt.unMute) {
                        try {
                            yt.unMute();
                            if (window.__yayc_playerVolume >= 0)
                                yt.setVolume(window.__yayc_playerVolume);
                        } catch (e) {}
                        continue;
                    }
                    var v = hosts[i].querySelector('video');
                    if (v) {
                        v.muted = false;
                        if (window.__yayc_playerVolume >= 0)
                            v.volume = window.__yayc_playerVolume / 100;
                    }
                }
            };

            // If the preview is torn down regardless, tell QML so it can drop the
            // pin instead of leaving hover frozen with nothing playing.
            setInterval(function() {
                if (!window.__yayc_pinned || !backend) return;
                if (!document.querySelector('ytd-video-preview video'))
                    backend.previewPinLost = (backend.previewPinLost || 0) + 1;
            }, 1000);
        })();
    "

    // Brings every player on the page in line with YAYC's global rate/volume:
    // the watch player, the active Shorts player, and the inline hover-preview
    // players (which have no UI of their own and inherit nothing). Values are
    // pushed in from WebView.qml as window globals, in the YouTube player API's
    // own scales (rate as a multiplier, volume 0-100).
    //
    // Reasserted on mutation, on 'loadeddata', and on a slow interval, because
    // preview players are created and destroyed as the cursor moves and YouTube
    // initialises each one with its own defaults. window.__yayc_applyPlayerSettings
    // is exposed so a push can take effect immediately instead of waiting a tick.
    property string script_applyPlayerSettings: "
        (function() {
            if (window.__yayc_playerctl) return;
            window.__yayc_playerctl = true;

            function collect() {
                return [].concat(
                    [].slice.call(document.querySelectorAll('ytd-watch-flexy ytd-player')),
                    [].slice.call(document.querySelectorAll('ytd-reel-video-renderer ytd-player')),
                    [].slice.call(document.querySelectorAll('ytd-video-preview ytd-player')));
            }

            function applyTo(host) {
                var r = window.__yayc_playerRate;
                var vol = window.__yayc_playerVolume;
                var yt = null;
                try { yt = host.getPlayer ? host.getPlayer() : null; } catch (e) { yt = null; }
                if (yt && yt.setPlaybackRate) {
                    // Player API is preferred: YouTube re-asserts its own values
                    // onto the underlying element, but respects the API.
                    try {
                        if (r > 0 && Math.abs(yt.getPlaybackRate() - r) > 0.001)
                            yt.setPlaybackRate(r);
                        // Never unmutes: unmuting without user activation makes
                        // Chromium pause the preview instead of playing it.
                        if (vol >= 0 && !yt.isMuted() && Math.abs(yt.getVolume() - vol) > 0.5)
                            yt.setVolume(vol);
                    } catch (e) {}
                    return;
                }
                var v = host.querySelector ? host.querySelector('video') : null;
                if (!v) return;
                if (r > 0 && v.playbackRate !== r)
                    v.playbackRate = r;
                if (vol >= 0 && !v.muted && Math.abs(v.volume - vol / 100) > 0.001)
                    v.volume = vol / 100;
            }

            function applyAll() {
                var l = collect();
                for (var i = 0; i < l.length; ++i)
                    applyTo(l[i]);
            }
            window.__yayc_applyPlayerSettings = applyAll;

            var obs = new MutationObserver(applyAll);
            obs.observe(document.body, { childList: true, subtree: true });
            document.addEventListener('loadeddata', applyAll, true);
            setInterval(applyAll, 1000);
        })();
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
        function __yaycRunShorts(backend) {
            try {
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
                let avatarEl = activeShort.querySelector('img.ytCoreImageHost.ytSpecAvatarShapeImage')
                              || activeShort.querySelector('.yt-spec-avatar-shape__image.ytCoreImageHost');
                backend.channelAvatar = avatarEl ? avatarEl.src : '';

                // Live playback state from player API
                backend.videoDuration = ytplayer.getDuration();
                backend.videoPosition = ytplayer.getCurrentTime();
                backend.playbackRate = ytplayer.getPlaybackRate();
                backend.playerState = ytplayer.getPlayerState();
                backend.volume = ytplayer.getVolume();
                backend.muted = ytplayer.isMuted();
                backend.videoQuality = ytplayer.getPlaybackQuality();
                backend.availableQualityLevels = ytplayer.getAvailableQualityLevels();
                backend.shortsSignal = videoData.video_id + ':' + ytplayer.getCurrentTime();
            } catch(e) {
                backend.shortsSignal = 'ERR:' + e.message;
            }
        }
        if (window.__yaycBackend) {
            setTimeout(function() { __yaycRunShorts(window.__yaycBackend); }, 100);
        } else {
            new QWebChannel(qt.webChannelTransport, function(channel) {
                window.__yaycBackend = channel.objects.backend;
                setTimeout(function() { __yaycRunShorts(window.__yaycBackend); }, 100);
            });
        }
    "

    // Rate/volume setters used to live here, one player at a time. They were
    // replaced by script_applyPlayerSettings, which addresses every player kind
    // (watch/Shorts/preview) from YAYC's global values.

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

    function getSeekByScript(deltaSec, isShorts) {
        var res = "
        setTimeout(function() {
" + getPlayer(isShorts) +
"            var t = ytplayer.getCurrentTime() + " + deltaSec + ";
            ytplayer.seekTo(t, true);
            var mp = document.getElementById('movie_player');
            if (mp && mp.classList.contains('ytp-autohide')) {
                mp.classList.remove('ytp-autohide');
                setTimeout(function() { mp.classList.add('ytp-autohide'); }, 1500);
            }
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
