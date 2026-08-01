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

    // Keeps a pinned preview alive. The Qt filter stops hover events before Chromium,
    // but Chromium also makes its own hover events from geometry (example: on scroll).
    // So block them inside the page too.
    //  - block is wide, not only inside ytd-video-preview: YouTube handlers sit on
    //    parent elements outside it, so a narrow block would miss them
    //  - cost: some page hover states stay stuck while pinned (only visual, temporary)
    property string script_previewPin: "
        (function() {
            if (window.__yayc_pinctl) return;
            window.__yayc_pinctl = true;

            // Preview players found by structure: any ytd-player that is not the watch
            // player and not a Short. Matching by tag ('ytd-video-preview ytd-player')
            // missed music video markup. That broke unmute and made the watchdog unpin
            // at once.
            window.__yayc_previewPlayers = function() {
                var all = document.querySelectorAll('ytd-player');
                var out = [];
                for (var i = 0; i < all.length; ++i) {
                    var p = all[i];
                    if (p.closest && (p.closest('ytd-watch-flexy')
                                      || p.closest('ytd-reel-video-renderer')))
                        continue;
                    out.push(p);
                }
                return out;
            };
            // No QWebChannel here on purpose. More channels on one transport fight over
            // message callbacks ('channel.execCallbacks[message.id] is not a function')
            // and can break other backend writes, like ad-skip. QML polls this counter.
            window.__yayc_pinLostSeq = 0;

            // Play/pause for the pinned preview. The pause backstop below resumes
            // anything YouTube pauses, so a wanted pause needs a flag it honours.
            window.__yayc_pinPaused = false;
            window.__yayc_setPreviewPaused = function(p) {
                window.__yayc_pinPaused = !!p;
                var hosts = window.__yayc_previewPlayers();
                for (var i = 0; i < hosts.length; ++i) {
                    var v = hosts[i].querySelector('video');
                    if (!v) continue;
                    try {
                        if (p) v.pause(); else v.play();
                    } catch (e) {}
                }
            };
            // Real state for the QML poll, so a preview that pauses on its own (end of
            // the clip) still shows the right icon.
            window.__yayc_previewPaused = function() {
                var hosts = window.__yayc_previewPlayers();
                for (var i = 0; i < hosts.length; ++i) {
                    var v = hosts[i].querySelector('video');
                    if (v) return !!v.paused;
                }
                return false;
            };

            function block(e) {
                if (window.__yayc_pinned)
                    e.stopImmediatePropagation();
            }
            // Block enter too, not only leave. On scroll Chromium checks what is under
            // the frozen pointer and sends over/enter there, which started the preview of
            // that thumbnail. Chromium makes these events, so only a page block stops them.
            var types = ['mouseleave', 'mouseout', 'pointerleave', 'pointerout',
                         'mouseover', 'mouseenter', 'pointerover', 'pointerenter',
                         'mousemove', 'pointermove'];
            for (var i = 0; i < types.length; ++i)
                document.addEventListener(types[i], block, true);

            // Right click elsewhere made YouTube close the preview (it reads
            // contextmenu/mousedown as 'user does something else'). Hide it from the page,
            // but do NOT preventDefault: Chromium must still send contextMenuRequested, so
            // our menu opens. Left button stays: it should unpin and navigate.
            function blockRightButton(e) {
                if (!window.__yayc_pinned)
                    return;
                if (e.type === 'contextmenu' || e.button === 2)
                    e.stopImmediatePropagation();
            }
            var rtypes = ['contextmenu', 'mousedown', 'mouseup', 'auxclick',
                          'pointerdown', 'pointerup'];
            for (var j = 0; j < rtypes.length; ++j)
                document.addEventListener(rtypes[j], blockRightButton, true);

            // Backstop: whatever route YouTube took to pause it, resume - unless the
            // pause was wanted (__yayc_pinPaused).
            document.addEventListener('pause', function(e) {
                if (!window.__yayc_pinned || window.__yayc_pinPaused) return;
                var v = e.target;
                if (!v || v.tagName !== 'VIDEO' || !v.closest) return;
                // Same structure test as __yayc_previewPlayers.
                if (v.closest('ytd-watch-flexy') || v.closest('ytd-reel-video-renderer'))
                    return;
                try { v.play(); } catch (err) {}
            }, true);

            // Preview audio: unmute once with the real prototype setter, then shadow
            // 'muted' on the element. YouTube writes then hit an empty setter, so the
            // native audio state never changes again.
            //  - no fight: unmute on a timer or on 'volumechange' fought YouTube re-mute,
            //    the player went to buffering, then playback died
            //  - uses the DOM API, not YouTube short names, so base.js rebuilds cannot
            //    break it. Done once per element (__yayc_audioForced)
            window.__yayc_forcePreviewAudio = function() {
                var proto = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, 'muted');
                if (!proto || !proto.set) return;
                var hosts = window.__yayc_previewPlayers();
                for (var i = 0; i < hosts.length; ++i) {
                    var v = hosts[i].querySelector('video');
                    if (!v || v.__yayc_audioForced) continue;
                    try {
                        proto.set.call(v, false);
                        Object.defineProperty(v, 'muted', {
                            configurable: true,
                            get: function() { return false; },
                            set: function() {}
                        });
                        if (window.__yayc_playerVolume >= 0)
                            v.volume = window.__yayc_playerVolume / 100;
                        v.__yayc_audioForced = true;
                    } catch (e) {}
                }
            };

            // Undo on unpin, so audio belongs to the pin only. YouTube reuses its preview
            // player, so keeping the shadow would make later hovers loud with no pin, and
            // the YouTube mute button would look dead.
            window.__yayc_releasePreviewAudio = function() {
                var proto = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, 'muted');
                var hosts = window.__yayc_previewPlayers();
                for (var i = 0; i < hosts.length; ++i) {
                    var v = hosts[i].querySelector('video');
                    if (!v || !v.__yayc_audioForced) continue;
                    try {
                        delete v.muted;              // drop the own property, prototype accessor is live again
                        if (proto && proto.set)
                            proto.set.call(v, true); // back to muted, as previews normally are
                        v.__yayc_audioForced = false;
                    } catch (e) {}
                }
            };

            // Preview gone anyway: tell QML to drop the pin, else hover stays frozen with
            // nothing playing. Needs 3 misses in a row, because the player can be missing
            // for a moment while YouTube rebuilds the DOM, and a wrong hit unpins silently.
            var misses = 0;
            setInterval(function() {
                if (!window.__yayc_pinned) { misses = 0; return; }
                var hosts = window.__yayc_previewPlayers();
                var alive = false;
                for (var i = 0; i < hosts.length && !alive; ++i)
                    alive = !!hosts[i].querySelector('video');
                if (alive) { misses = 0; return; }
                if (++misses >= 3) {
                    misses = 0;
                    window.__yayc_pinLostSeq = (window.__yayc_pinLostSeq || 0) + 1;
                }
            }, 1000);
        })();
    "

    // Applies YAYC global rate/volume to every player: watch, active Short, and hover
    // previews (previews have no UI and inherit nothing). Values come from WebView.qml as
    // window globals, in player API scales (rate multiplier, volume 0-100).
    //  - reasserted on mutation, on 'loadeddata' and on a slow interval: preview players are
    //    created and destroyed as the cursor moves and YouTube initialises each with its own
    //    defaults
    //  - __yayc_applyPlayerSettings is exported, so a push takes effect at once
    //  - the other direction is a 'ratechange' listener: a rate set in YouTube's own UI
    //    becomes the new target instead of being reasserted away, and reaches the controls
    //    on the next timePuller pull
    property string script_applyPlayerSettings: "
        (function() {
            if (window.__yayc_playerctl) return;
            window.__yayc_playerctl = true;

            function mains() {
                return [].concat(
                    [].slice.call(document.querySelectorAll('ytd-watch-flexy ytd-player')),
                    [].slice.call(document.querySelectorAll('ytd-reel-video-renderer ytd-player')));
            }

            function collect() {
                // Defined by script_previewPin, so both scripts agree what a preview is.
                // Guard is only for injection order. No tag fallback: tag matching is what
                // missed music video previews.
                var previews = window.__yayc_previewPlayers
                        ? window.__yayc_previewPlayers() : [];
                return mains().concat(previews);
            }

            function applyTo(host) {
                var r = window.__yayc_playerRate;
                var vol = window.__yayc_playerVolume;
                var v = host.querySelector ? host.querySelector('video') : null;

                // Pinned preview with forced audio: set the element directly. The player
                // API is useless here - we unmuted by shadowing 'muted' on the element, so
                // the API still reports isMuted() == true and the mute guard below would
                // skip every volume change (slider looked dead while pinned).
                if (v && v.__yayc_audioForced) {
                    if (r > 0 && v.playbackRate !== r)
                        v.playbackRate = r;
                    if (vol >= 0 && Math.abs(v.volume - vol / 100) > 0.001)
                        v.volume = vol / 100;
                    return;
                }

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

            // A source change resets the player to YouTube's own rate: next video, ad start,
            // ad end. Those must be forced back, so ratechange is ignored around them.
            var transitionUntil = 0;
            function noteTransition() { transitionUntil = Date.now() + 2500; }
            document.addEventListener('loadstart', noteTransition, true);
            document.addEventListener('emptied', noteTransition, true);

            // Rate changed on the watch/Shorts player by something other than applyTo():
            // YouTube's speed menu or its < > shortcuts. Our own writes always land on the
            // target, so a rate that ends up away from it can only be external - adopt it,
            // and the QML side picks it up on its next pull.
            document.addEventListener('ratechange', function(e) {
                var v = e.target;
                if (!v || !v.closest || !(v.playbackRate > 0))
                    return;
                if (Date.now() < transitionUntil)
                    return;
                var host = v.closest('ytd-player');
                // Previews get no vote: each starts at YouTube defaults, which would drag the
                // global rate back to 1 every time the cursor crosses a thumbnail.
                if (!host || mains().indexOf(host) < 0)
                    return;
                if (host.querySelector('.ad-showing'))
                    return;
                if (Math.abs(v.playbackRate - window.__yayc_playerRate) > 0.001)
                    window.__yayc_playerRate = v.playbackRate;
            }, true);

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
