#pragma once

#include <windows.h>
#include <appmodel.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Nilesoft::Shell::WindowsTerminal
{
	struct Profile
	{
		std::wstring name;
		std::wstring selector;
		std::wstring icon;
		std::wstring source;
		std::wstring commandline;
		bool iconDefined = false;
	};

	namespace detail
	{
		struct Json
		{
			enum class Type { Null, Boolean, Number, String, Array, Object } type = Type::Null;
			bool boolean = false;
			std::wstring text;
			std::vector<Json> array;
			std::map<std::wstring, Json> object;

			const Json &get(const wchar_t *key) const
			{
				static const Json missing;
				auto it = object.find(key);
				return it == object.end() ? missing : it->second;
			}
		};

		class JsonReader
		{
			std::wstring_view input;
			size_t pos = 0;

			[[noreturn]] static void invalid() { throw std::runtime_error("Invalid Terminal settings"); }
			wchar_t peek() const { return pos < input.size() ? input[pos] : 0; }
			bool take(wchar_t ch)
			{
				if(peek() != ch) return false;
				++pos;
				return true;
			}
			void whitespace()
			{
				for(;;)
				{
					while(peek() == L' ' || peek() == L'\t' || peek() == L'\r' || peek() == L'\n') ++pos;
					if(input.substr(pos, 2) == L"//")
					{
						pos += 2;
						while(pos < input.size() && peek() != L'\n' && peek() != L'\r') ++pos;
					}
					else if(input.substr(pos, 2) == L"/*")
					{
						auto end = input.find(L"*/", pos + 2);
						if(end == std::wstring_view::npos) invalid();
						pos = end + 2;
					}
					else return;
				}
			}
			unsigned hex4()
			{
				unsigned value = 0;
				for(int i = 0; i < 4; ++i)
				{
					auto ch = peek();
					unsigned digit;
					if(ch >= L'0' && ch <= L'9') digit = ch - L'0';
					else if(ch >= L'a' && ch <= L'f') digit = ch - L'a' + 10;
					else if(ch >= L'A' && ch <= L'F') digit = ch - L'A' + 10;
					else invalid();
					value = value * 16 + digit;
					++pos;
				}
				return value;
			}
			std::wstring string()
			{
				if(!take(L'"')) invalid();
				std::wstring result;
				while(!take(L'"'))
				{
					auto ch = peek();
					if(ch < 0x20) invalid();
					++pos;
					if(ch == L'\\')
					{
						ch = peek();
						++pos;
						switch(ch)
						{
						case L'"': case L'\\': case L'/': break;
						case L'b': ch = L'\b'; break;
						case L'f': ch = L'\f'; break;
						case L'n': ch = L'\n'; break;
						case L'r': ch = L'\r'; break;
						case L't': ch = L'\t'; break;
						case L'u':
						{
							auto code = hex4();
							if(code >= 0xD800 && code <= 0xDBFF)
							{
								if(!take(L'\\') || !take(L'u')) invalid();
								auto low = hex4();
								if(low < 0xDC00 || low > 0xDFFF) invalid();
								result += static_cast<wchar_t>(code);
								code = low;
							}
							else if(code >= 0xDC00 && code <= 0xDFFF) invalid();
							ch = static_cast<wchar_t>(code);
							break;
						}
						default: invalid();
						}
					}
					result += ch;
				}
				return result;
			}
			void digits()
			{
				if(peek() < L'0' || peek() > L'9') invalid();
				while(peek() >= L'0' && peek() <= L'9') ++pos;
			}
			Json value(unsigned depth)
			{
				if(depth > 64) invalid();
				whitespace();
				Json result;
				if(peek() == L'{' || peek() == L'[')
				{
					bool object = take(L'{');
					if(!object) ++pos;
					result.type = object ? Json::Type::Object : Json::Type::Array;
					auto close = object ? L'}' : L']';
					whitespace();
					while(!take(close))
					{
						if(object)
						{
							auto key = string();
							whitespace();
							if(!take(L':')) invalid();
							result.object[std::move(key)] = value(depth + 1);
						}
						else result.array.push_back(value(depth + 1));
						whitespace();
						if(take(close)) break;
						if(!take(L',')) invalid();
						whitespace();
					}
				}
				else if(peek() == L'"')
				{
					result.type = Json::Type::String;
					result.text = string();
				}
				else if(input.substr(pos, 4) == L"true" || input.substr(pos, 5) == L"false")
				{
					result.type = Json::Type::Boolean;
					result.boolean = peek() == L't';
					pos += result.boolean ? 4 : 5;
				}
				else if(input.substr(pos, 4) == L"null") pos += 4;
				else
				{
					result.type = Json::Type::Number;
					take(L'-');
					if(!take(L'0')) digits();
					if(take(L'.')) digits();
					if(take(L'e') || take(L'E'))
					{
						if(!take(L'+')) take(L'-');
						digits();
					}
				}
				return result;
			}
		public:
			explicit JsonReader(std::wstring_view text) : input(text) {}
			Json read()
			{
				try
				{
					if(peek() == 0xFEFF) ++pos;
					auto result = value(0);
					whitespace();
					if(pos != input.size()) invalid();
					return result;
				}
				catch(const std::runtime_error &) { return {}; }
			}
		};

		inline bool valid_text(const std::wstring &text)
		{
			return text.find_first_not_of(L" \t\r\n") != std::wstring::npos &&
				std::none_of(text.begin(), text.end(), [](wchar_t ch) { return ch < 0x20; });
		}

		inline bool guid(std::wstring_view text)
		{
			if(text.size() == 38 && text.front() == L'{' && text.back() == L'}') text = text.substr(1, 36);
			if(text.size() != 36) return false;
			for(size_t i = 0; i < text.size(); ++i)
			{
				auto ch = text[i];
				if(i == 8 || i == 13 || i == 18 || i == 23) { if(ch != L'-') return false; }
				else if(!((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F'))) return false;
			}
			return true;
		}
	}

	inline std::vector<Profile> Parse(std::wstring_view text)
	{
		auto root = detail::JsonReader(text).read();
		const auto &profiles = root.get(L"profiles");
		const auto &list = profiles.type == detail::Json::Type::Array ? profiles : profiles.get(L"list");
		const auto &defaults = profiles.get(L"defaults");
		std::vector<Profile> result;
		for(const auto &entry : list.array)
		{
			const auto &name = entry.get(L"name");
			const auto &hidden = entry.object.count(L"hidden") ? entry.get(L"hidden") : defaults.get(L"hidden");
			if(name.type != detail::Json::Type::String || !detail::valid_text(name.text) ||
				(hidden.type != detail::Json::Type::Null && hidden.type != detail::Json::Type::Boolean) || hidden.boolean) continue;
			const auto &guid = entry.get(L"guid");
			if(guid.type != detail::Json::Type::Null && (guid.type != detail::Json::Type::String || !detail::guid(guid.text))) continue;
			auto selector = guid.type == detail::Json::Type::String ? guid.text : name.text;
			if(detail::guid(selector) && selector.front() != L'{') selector = L"{" + selector + L"}";
			if(std::any_of(result.begin(), result.end(), [&](const Profile &p) { return _wcsicmp(p.selector.c_str(), selector.c_str()) == 0; })) continue;
			const auto &icon = entry.object.count(L"icon") ? entry.get(L"icon") : defaults.get(L"icon");
			result.push_back({ name.text, std::move(selector), icon.text,
				entry.get(L"source").text, entry.get(L"commandline").text,
				entry.object.count(L"icon") != 0 || defaults.object.count(L"icon") != 0 });
		}
		return result;
	}

	inline std::wstring Expand(const std::wstring &text)
	{
		auto size = ::ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
		if(!size) return {};
		std::wstring result(size, L'\0');
		auto written = ::ExpandEnvironmentStringsW(text.c_str(), result.data(), size);
		if(!written || written > size) return {};
		result.resize(written - 1);
		return result;
	}

	inline std::wstring SettingsPath()
	{
		return Expand(L"%LOCALAPPDATA%\\Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\settings.json");
	}

	inline std::wstring ReadText(const std::wstring &path)
	{
		auto handle = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if(handle == INVALID_HANDLE_VALUE) return {};
		std::unique_ptr<void, decltype(&::CloseHandle)> file(handle, ::CloseHandle);
		LARGE_INTEGER size{};
		if(!::GetFileSizeEx(handle, &size) || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) return {};
		std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
		DWORD read = 0;
		if(!::ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size()) return {};
		auto length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(read), nullptr, 0);
		if(!length) return {};
		std::wstring text(length, L'\0');
		if(!::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(read), text.data(), length)) return {};
		return text;
	}

	inline std::vector<Profile> Load(const std::wstring &path = SettingsPath())
	{
		return Parse(ReadText(path));
	}

	inline std::wstring PackageDirectory()
	{
		// Resolve dynamically for Windows 7 compatibility.
		auto kernel = ::GetModuleHandleW(L"kernel32.dll");
		auto find = reinterpret_cast<decltype(&::FindPackagesByPackageFamily)>(::GetProcAddress(kernel, "FindPackagesByPackageFamily"));
		auto getPath = reinterpret_cast<decltype(&::GetPackagePathByFullName)>(::GetProcAddress(kernel, "GetPackagePathByFullName"));
		auto getId = reinterpret_cast<decltype(&::PackageIdFromFullName)>(::GetProcAddress(kernel, "PackageIdFromFullName"));
		if(!find || !getPath || !getId) return {};
		constexpr auto family = L"Microsoft.WindowsTerminal_8wekyb3d8bbwe";
		UINT32 count = 0, length = 0;
		if(find(family, PACKAGE_FILTER_HEAD, &count, nullptr, &length, nullptr, nullptr) != ERROR_INSUFFICIENT_BUFFER || !count) return {};
		std::vector<wchar_t> buffer(length);
		std::vector<PWSTR> names(count);
		if(find(family, PACKAGE_FILTER_HEAD, &count, names.data(), &length, buffer.data(), nullptr) != ERROR_SUCCESS) return {};
		std::wstring result;
		ULONGLONG version = 0;
		for(UINT32 i = 0; i < count; ++i)
		{
			UINT32 size = 0;
			if(getId(names[i], PACKAGE_INFORMATION_BASIC, &size, nullptr) != ERROR_INSUFFICIENT_BUFFER) continue;
			std::vector<BYTE> idBuffer(size);
			if(getId(names[i], PACKAGE_INFORMATION_BASIC, &size, idBuffer.data()) != ERROR_SUCCESS) continue;
			auto id = reinterpret_cast<const PACKAGE_ID *>(idBuffer.data());
			if(id->resourceId && *id->resourceId) continue;
			if(!result.empty() && id->version.Version <= version) continue;
			size = 0;
			if(getPath(names[i], &size, nullptr) != ERROR_INSUFFICIENT_BUFFER) continue;
			std::wstring path(size, L'\0');
			if(getPath(names[i], &size, path.data()) != ERROR_SUCCESS || !size) continue;
			path.resize(size - 1);
			auto attributes = ::GetFileAttributesW((path + L"\\WindowsTerminal.exe").c_str());
			if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
			result = std::move(path);
			version = id->version.Version;
		}
		return result;
	}

	inline std::wstring Executable()
	{
		auto path = Expand(L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\wt.exe");
		auto attributes = ::GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) ? path : L"";
	}

	inline std::wstring IconPath(const std::wstring &icon, const std::wstring &packageDirectory = {}, const std::wstring &baseDirectory = {})
	{
		if(!detail::valid_text(icon)) return {};
		auto path = Expand(icon);
		bool packaged = false;
		if(_wcsnicmp(path.c_str(), L"ms-appdata:///", 14) == 0)
		{
			auto relative = path.substr(14);
			if(_wcsnicmp(relative.c_str(), L"local/", 6) == 0) relative.replace(0, 6, L"LocalState/");
			else if(_wcsnicmp(relative.c_str(), L"roaming/", 8) == 0) relative.replace(0, 8, L"RoamingState/");
			else return {};
			path = Expand(L"%LOCALAPPDATA%\\Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\") + relative;
		}
		else if(_wcsnicmp(path.c_str(), L"ms-appx:///", 11) == 0)
		{
			if(packageDirectory.empty()) return {};
			path = packageDirectory + L"\\" + path.substr(11);
			packaged = true;
		}
		else if(path.find(L"://") != std::wstring::npos) return {};
		else if(::PathIsRelativeW(path.c_str()))
		{
			auto settings = SettingsPath();
			path = (baseDirectory.empty() ? settings.substr(0, settings.find_last_of(L'\\') + 1) : baseDirectory + L"\\") + path;
		}
		if(icon.find(L":///") != std::wstring::npos)
		{
			if(FAILED(::UrlUnescapeW(path.data(), nullptr, nullptr, URL_UNESCAPE_INPLACE | URL_UNESCAPE_AS_UTF8))) return {};
			path.resize(wcslen(path.c_str()));
		}
		std::replace(path.begin(), path.end(), L'/', L'\\');
		auto attributes = ::GetFileAttributesW(path.c_str());
		if(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return path;
		// Packaged images may only have qualified scale variants on disk.
		if(packaged)
		{
			auto extension = path.find_last_of(L'.');
			if(extension != std::wstring::npos)
			{
				for(auto scale : { L"100", L"200", L"400", L"150", L"125" })
				{
					auto variant = path.substr(0, extension) + L".scale-" + scale + path.substr(extension);
					attributes = ::GetFileAttributesW(variant.c_str());
					if(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return variant;
				}
			}
		}
		return {};
	}

	namespace detail
	{
		inline std::vector<std::wstring> children(const std::wstring &directory, bool directories)
		{
			std::vector<std::wstring> result;
			if(directory.empty()) return result;
			WIN32_FIND_DATAW data{};
			auto handle = ::FindFirstFileW((directory + L"\\*").c_str(), &data);
			if(handle == INVALID_HANDLE_VALUE) return result;
			std::unique_ptr<void, decltype(&::FindClose)> search(handle, ::FindClose);
			do
			{
				if(wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
				if(bool(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == directories)
					result.push_back(directory + L"\\" + data.cFileName);
			} while(::FindNextFileW(handle, &data));
			std::sort(result.begin(), result.end());
			return result;
		}

		inline std::wstring profile_guid(const std::wstring &seed)
		{
			// Terminal's DynamicProfileUtils uses UUID v5 over UTF-16LE seed bytes.
			constexpr BYTE namespaceId[] = { 0x2b, 0xde, 0x4a, 0x90, 0xd0, 0x5f, 0x40, 0x1c, 0x94, 0x92, 0xe4, 0x08, 0x84, 0xea, 0xd1, 0xd8 };
			HCRYPTPROV provider = 0;
			if(!::CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return {};
			HCRYPTHASH hash = 0;
			BYTE digest[20]{};
			DWORD size = sizeof(digest);
			bool ok = ::CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash) &&
				::CryptHashData(hash, namespaceId, sizeof(namespaceId), 0) &&
				::CryptHashData(hash, reinterpret_cast<const BYTE *>(seed.data()), static_cast<DWORD>(seed.size() * sizeof(wchar_t)), 0) &&
				::CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0);
			if(hash) ::CryptDestroyHash(hash);
			::CryptReleaseContext(provider, 0);
			if(!ok) return {};
			digest[6] = (digest[6] & 0x0f) | 0x50;
			digest[8] = (digest[8] & 0x3f) | 0x80;
			constexpr auto hex = L"0123456789abcdef";
			std::wstring result = L"{";
			for(size_t i = 0; i < 16; ++i)
			{
				if(i == 4 || i == 6 || i == 8 || i == 10) result += L'-';
				result += hex[digest[i] >> 4];
				result += hex[digest[i] & 15];
			}
			return result + L'}';
		}

		struct IconLayer
		{
			std::wstring selector, source, icon, directory;
			bool updates = false;
		};

		inline void icon_layers(std::vector<IconLayer> &layers, const std::wstring &path, const std::wstring &source)
		{
			auto root = JsonReader(ReadText(path)).read();
			const auto &profiles = root.get(L"profiles");
			const auto &list = profiles.type == Json::Type::Array ? profiles : profiles.get(L"list");
			for(const auto &entry : list.array)
			{
				if(!entry.object.count(L"icon")) continue;
				const auto &icon = entry.get(L"icon");
				if(icon.type != Json::Type::String && icon.type != Json::Type::Null) continue;
				bool updates = entry.object.count(L"updates") != 0;
				const auto &id = entry.get(updates ? L"updates" : L"guid");
				if(id.type != Json::Type::String || !guid(id.text)) continue;
				auto selector = id.text.front() == L'{' ? id.text : L"{" + id.text + L"}";
				layers.push_back({ std::move(selector), source, icon.text, path.substr(0, path.find_last_of(L"\\/")), updates });
			}
		}

		inline std::wstring generated_icon(const Profile &profile)
		{
			if(profile.source == L"Windows.Terminal.PowershellCore")
			{
				auto hint = profile.name + L" " + profile.commandline;
				std::transform(hint.begin(), hint.end(), hint.begin(), [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
				return hint.find(L"preview") == std::wstring::npos ? L"ms-appx:///ProfileIcons/pwsh.png" : L"ms-appx:///ProfileIcons/pwsh-preview.png";
			}
			if(profile.source == L"Windows.Terminal.Wsl")
				return L"ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png";
			if(profile.source == L"Windows.Terminal.Azure")
				return L"ms-appx:///ProfileIcons/{b453ae62-4e3d-5e58-b989-0a998ec441b8}.png";
			return guid(profile.selector) ? L"ms-appx:///ProfileIcons/" + profile.selector + L".png" : L"";
		}
	}

	inline std::vector<std::wstring> FragmentDirectories()
	{
		return { Expand(L"%LOCALAPPDATA%\\Microsoft\\Windows Terminal\\Fragments"),
			Expand(L"%ProgramData%\\Microsoft\\Windows Terminal\\Fragments") };
	}

	inline void ResolveIcons(std::vector<Profile> &profiles, const std::wstring &packageDirectory,
		const std::vector<std::wstring> &fragmentDirectories = FragmentDirectories(),
		const std::wstring &vsInstances = Expand(L"%ProgramData%\\Microsoft\\VisualStudio\\Packages\\_Instances"))
	{
		if(profiles.empty()) return;
		// User profile > profiles.defaults > fragment updates > base profile.
		std::vector<detail::IconLayer> layers;
		if(!packageDirectory.empty()) detail::icon_layers(layers, packageDirectory + L"\\defaults.json", L"");
		for(const auto &path : detail::children(vsInstances, true))
		{
			auto instance = path.substr(path.find_last_of(L'\\') + 1);
			layers.push_back({ detail::profile_guid(L"VsDevCmd" + instance), L"Windows.Terminal.VisualStudio", L"ms-appx:///ProfileIcons/vs-cmd.png", L"" });
			layers.push_back({ detail::profile_guid(L"VsDevShell" + instance), L"Windows.Terminal.VisualStudio", L"ms-appx:///ProfileIcons/vs-powershell.png", L"" });
		}
		for(const auto &root : fragmentDirectories)
		{
			for(const auto &directory : detail::children(root, true))
			{
				auto source = directory.substr(directory.find_last_of(L'\\') + 1);
				for(const auto &path : detail::children(directory, false))
					if(_wcsicmp(::PathFindExtensionW(path.c_str()), L".json") == 0) detail::icon_layers(layers, path, source);
			}
		}
		for(auto &profile : profiles)
		{
			std::wstring directory;
			if(!profile.iconDefined)
			{
				const detail::IconLayer *inherited = nullptr;
				for(const auto &layer : layers)
					if(_wcsicmp(layer.selector.c_str(), profile.selector.c_str()) == 0 &&
						(layer.updates || layer.source == profile.source) && (!inherited || layer.updates)) inherited = &layer;
				profile.icon = inherited ? inherited->icon : detail::generated_icon(profile);
				if(inherited) directory = inherited->directory;
			}
			profile.icon = IconPath(profile.icon, packageDirectory, directory);
		}
	}

	inline std::wstring QuoteArgument(std::wstring_view value)
	{
		std::wstring result = L"\"";
		size_t slashes = 0;
		for(auto ch : value)
		{
			if(ch == L'\\') { ++slashes; continue; }
			result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
			slashes = 0;
			// wt splits commands on semicolons after Windows removes quotes.
			if(ch == L';') result += L'\\';
			result += ch;
		}
		result.append(slashes * 2, L'\\');
		return result + L'"';
	}

	inline std::wstring Arguments(const Profile &profile, std::wstring_view directory)
	{
		return L"-p " + QuoteArgument(profile.selector) + L" -d " + QuoteArgument(directory.empty() ? L"." : directory);
	}
}
