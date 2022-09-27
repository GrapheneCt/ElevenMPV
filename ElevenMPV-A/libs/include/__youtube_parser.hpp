#pragma once
#include <vector>
#include <string>
#include <map>

#define TT_PORTABLE

#ifdef TT_PRX
#define PRX_EXPORT __declspec(dllexport)
#else
#define PRX_EXPORT
#endif

PRX_EXPORT void youtube_change_content_language(std::string language_code);

// util function
PRX_EXPORT int youtube_set_audio_bitrate_limit(int limit);
PRX_EXPORT std::string youtube_get_video_thumbnail_url_by_id(const std::string &id);
PRX_EXPORT std::string youtube_get_video_thumbnail_hq_url_by_id(const std::string &id);
PRX_EXPORT std::string youtube_get_video_url_by_id(const std::string &id);
PRX_EXPORT std::string get_video_id_from_thumbnail_url(const std::string &url);
PRX_EXPORT bool youtube_is_valid_video_id(const std::string &id);
PRX_EXPORT bool is_youtube_url(const std::string &url);
PRX_EXPORT bool is_youtube_thumbnail_url(const std::string &url);

enum class YouTubePageType {
	VIDEO,
	CHANNEL,
	SEARCH,
	INVALID
};
PRX_EXPORT YouTubePageType youtube_get_page_type(std::string url);

#ifdef TT_PORTABLE
struct YouTubeChannelSuccinct {
	std::string name;
	std::string url;
	std::string icon_url;
	std::string subscribers;
	std::string video_num;
};
struct YouTubeVideoSuccinct {
	std::string url;
	std::string title;
	std::string duration_text;
	std::string publish_date;
	std::string views_str;
	std::string author;
	std::string thumbnail_url;
};
struct YouTubePlaylistSuccinct {
	std::string url;
	std::string title;
	std::string video_count_str;
	std::string thumbnail_url;
};

struct YouTubeSuccinctItem {
	// TODO : use union or std::variant
	enum {
		VIDEO,
		CHANNEL,
		PLAYLIST
	} type;
	YouTubeVideoSuccinct video;
	YouTubeChannelSuccinct channel;
	YouTubePlaylistSuccinct playlist;

	YouTubeSuccinctItem() = default;
	YouTubeSuccinctItem(YouTubeVideoSuccinct video) : type(VIDEO), video(video) {}
	YouTubeSuccinctItem(YouTubeChannelSuccinct channel) : type(CHANNEL), channel(channel) {}
	YouTubeSuccinctItem(YouTubePlaylistSuccinct playlist) : type(PLAYLIST), playlist(playlist) {}

	std::string get_url() const { return type == VIDEO ? video.url : type == CHANNEL ? channel.url : playlist.url; }
	std::string get_thumbnail_url() const { return type == VIDEO ? video.thumbnail_url : type == CHANNEL ? channel.icon_url : playlist.thumbnail_url; }
	std::string get_name() const { return type == VIDEO ? video.title : type == CHANNEL ? channel.name : playlist.title; }
};


struct YouTubeSearchResult {
	std::string error;
	int estimated_result_num;
	std::vector<YouTubeSuccinctItem> results;

	std::string continue_token;
	std::string continue_key;

	bool has_continue() const { return continue_token != "" && continue_key != ""; }
};
PRX_EXPORT YouTubeSearchResult *youtube_parse_search(std::string url);
PRX_EXPORT YouTubeSearchResult *youtube_parse_search(char *url);
// takes the previous result, returns the new result with both old items and new items
PRX_EXPORT YouTubeSearchResult *youtube_continue_search(const YouTubeSearchResult &prev_result);
PRX_EXPORT YouTubeSearchResult *youtube_parse_search_word(char *search_word);


struct YouTubeVideoDetail {
	std::string error;
	std::string url;
	std::string title;
	YouTubeChannelSuccinct author;
	std::string audio_stream_url;
	int duration_ms;
	bool is_livestream;
	enum class LivestreamType {
		PREMIERE,
		LIVESTREAM,
	};
	LivestreamType livestream_type;
	bool is_upcoming;
	std::string playability_status;
	std::string playability_reason;
	int stream_fragment_len; // used only for livestreams

	struct Playlist {
		std::string id;
		std::string title;
		std::string author_name;
		int total_videos;
		std::vector<YouTubeVideoSuccinct> videos;
		int selected_index;
	};
	Playlist playlist;

	std::string continue_key; // innertube key

	bool needs_timestamp_adjusting() const { return is_livestream && livestream_type == LivestreamType::PREMIERE; }
	bool is_playable() const { return playability_status == "OK" && ((audio_stream_url != "")); }
};
// this function does not load comments; call youtube_video_page_load_more_comments() if necessary
PRX_EXPORT YouTubeVideoDetail *youtube_parse_video_page(std::string url);
PRX_EXPORT YouTubeVideoDetail *youtube_parse_video_page(char *url);

struct YouTubeChannelDetail {
	std::string id;
	std::string error;
	std::string name;
	std::string url;
	std::string url_original;
	std::string icon_url;
	std::string banner_url;
	std::string description;
	std::string subscriber_count_str;
	std::vector<YouTubeVideoSuccinct> videos;

	std::string continue_token;
	std::string continue_key;

	bool has_continue() const { return continue_token != "" && continue_key != ""; }
};
PRX_EXPORT YouTubeChannelDetail *youtube_parse_channel_page(std::string url);
PRX_EXPORT YouTubeChannelDetail *youtube_parse_channel_page(char *url);
// takes the previous result, returns the new result with both old items and new items
PRX_EXPORT YouTubeChannelDetail *youtube_channel_page_continue(const YouTubeChannelDetail &prev_result);

PRX_EXPORT void youtube_destroy_struct(YouTubeChannelDetail *s);
PRX_EXPORT void youtube_destroy_struct(YouTubeVideoDetail *s);
PRX_EXPORT void youtube_destroy_struct(YouTubeSearchResult *s);

PRX_EXPORT void youtube_change_content_language(const char *language_code);

// util function
PRX_EXPORT void youtube_get_video_thumbnail_url_by_id(const char *id, char *url, int urlLen);
PRX_EXPORT void youtube_get_video_thumbnail_hq_url_by_id(const char *id, char *url, int urlLen);
PRX_EXPORT void youtube_get_video_url_by_id(const char *id, char *url, int urlLen);
PRX_EXPORT void get_video_id_from_thumbnail_url(const char *url, char *id, int idLen);
PRX_EXPORT bool youtube_is_valid_video_id(const char *id);
PRX_EXPORT bool is_youtube_url(const char *url);
PRX_EXPORT bool is_youtube_thumbnail_url(const char *url);
PRX_EXPORT YouTubePageType youtube_get_page_type(const char *url);

#else
#endif


