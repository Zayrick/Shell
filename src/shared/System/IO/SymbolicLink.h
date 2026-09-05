#pragma once

#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <vector>
#include <Resource.h>

namespace Nilesoft::IO
{
	class SymbolicLink
	{
		static DWORD FullPath(const std::wstring &path, std::wstring &result)
		{
			if(path.empty())
				return ERROR_INVALID_NAME;
			auto size = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
			if(!size)
				return ::GetLastError();
			std::vector<wchar_t> buffer(size);
			auto length = ::GetFullPathNameW(path.c_str(), size, buffer.data(), nullptr);
			if(!length)
				return ::GetLastError();
			if(length >= size)
				return ERROR_INSUFFICIENT_BUFFER;
			result.assign(buffer.data(), length);
			return ERROR_SUCCESS;
		}

		static std::wstring NativePath(const std::wstring &path)
		{
			if(path.compare(0, 4, L"\\\\?\\") == 0)
				return path;
			if(path.compare(0, 2, L"\\\\") == 0)
				return L"\\\\?\\UNC\\" + path.substr(2);
			return L"\\\\?\\" + path;
		}

	public:
		// Passing nullptr checks availability without copying the paths.
		static DWORD ReadClipboard(std::vector<std::wstring> *files = nullptr)
		{
			if(files)
				files->clear();
			if(!::IsClipboardFormatAvailable(CF_HDROP))
				return ERROR_NO_MORE_ITEMS;
			if(!::OpenClipboard(nullptr))
				return ERROR_ACCESS_DENIED;
			struct ClipboardLock { ~ClipboardLock() { ::CloseClipboard(); } } lock;

			auto effectFormat = ::RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
			if(auto effectData = ::GetClipboardData(effectFormat))
			{
				if(::GlobalSize(effectData) < sizeof(DWORD))
					return ERROR_INVALID_DATA;
				auto effect = static_cast<const DWORD *>(::GlobalLock(effectData));
				if(!effect)
					return ERROR_INVALID_DATA;
				bool cut = (*effect & DROPEFFECT_MOVE) != 0;
				::GlobalUnlock(effectData);
				if(cut)
					return ERROR_NO_MORE_ITEMS;
			}

			auto drop = static_cast<HDROP>(::GetClipboardData(CF_HDROP));
			if(!drop)
				return ERROR_INVALID_DATA;
			auto count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
			if(!count)
				return ERROR_NO_MORE_ITEMS;
			if(files)
			{
				for(UINT i = 0; i < count; ++i)
				{
					auto length = ::DragQueryFileW(drop, i, nullptr, 0);
					if(!length)
						return ERROR_INVALID_DATA;
					std::vector<wchar_t> path(length + 1);
					if(::DragQueryFileW(drop, i, path.data(), length + 1) != length)
						return ERROR_INVALID_DATA;
					files->emplace_back(path.data(), length);
				}
			}
			return ERROR_SUCCESS;
		}

		// CommandLineToArgvW requires trailing backslashes to be escaped inside quotes.
		static std::wstring QuoteArgument(const std::wstring &argument)
		{
			std::wstring result = L"\"";
			size_t slashes = 0;
			for(auto ch : argument)
			{
				if(ch == L'\\')
					++slashes;
				else
				{
					result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
					result += ch;
					slashes = 0;
				}
			}
			result.append(slashes * 2, L'\\');
			return result + L'"';
		}

		static DWORD Launch(const std::wstring &executable, const std::wstring &directory,
			const std::vector<std::wstring> &files, HWND owner, bool elevate = false, size_t first = 0)
		{
			if(first >= files.size())
				return ERROR_NO_MORE_ITEMS;
			// Resolve paths before the helper changes process or elevation context.
			std::wstring destination;
			if(auto error = FullPath(directory, destination); error != ERROR_SUCCESS)
				return error;
			std::wstring arguments = L"--paste-symlink " + QuoteArgument(destination);
			for(size_t i = first; i < files.size(); ++i)
			{
				std::wstring source;
				if(auto error = FullPath(files[i], source); error != ERROR_SUCCESS)
					return error;
				arguments += L" " + QuoteArgument(source);
				if(executable.size() + arguments.size() + 4 >= 32767)
					return ERROR_FILENAME_EXCED_RANGE;
			}
			SHELLEXECUTEINFOW info{};
			info.cbSize = sizeof(info);
			info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
			info.hwnd = owner;
			info.lpVerb = elevate ? L"runas" : L"open";
			info.lpFile = executable.c_str();
			info.lpParameters = arguments.c_str();
			info.nShow = SW_HIDE;
			return ::ShellExecuteExW(&info) ? ERROR_SUCCESS : ::GetLastError();
		}

		static DWORD Create(const std::wstring &source, const std::wstring &directory)
		{
			auto nativeTarget = NativePath(source);
			auto attributes = ::GetFileAttributesW(nativeTarget.c_str());
			if(attributes == INVALID_FILE_ATTRIBUTES)
				return ::GetLastError();

			auto sourcePath = std::filesystem::path(source);
			if(!sourcePath.has_filename())
				sourcePath = sourcePath.parent_path();
			auto name = sourcePath.filename().wstring();
			if(name.empty())
				return ERROR_INVALID_NAME;
			bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			auto stem = isDirectory ? name : sourcePath.stem().wstring();
			auto extension = isDirectory ? std::wstring() : sourcePath.extension().wstring();
			DWORD flags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;

			for(unsigned index = 1;; ++index)
			{
				auto candidate = index == 1 ? name : stem + L" (" + std::to_wstring(index) + L")" + extension;
				auto link = (std::filesystem::path(directory) / candidate).wstring();
				auto nativeLink = NativePath(link);
				auto success = ::CreateSymbolicLinkW(nativeLink.c_str(), nativeTarget.c_str(),
					flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
				auto error = success ? ERROR_SUCCESS : ::GetLastError();
				// Windows 7/8 and older Windows 10 builds do not recognize the new flag.
				if(!success && error == ERROR_INVALID_PARAMETER)
				{
					success = ::CreateSymbolicLinkW(nativeLink.c_str(), nativeTarget.c_str(), flags);
					error = success ? ERROR_SUCCESS : ::GetLastError();
				}
				if(success)
				{
					::SHChangeNotify(isDirectory ? SHCNE_MKDIR : SHCNE_CREATE, SHCNF_PATHW,
						link.c_str(), nullptr);
					return ERROR_SUCCESS;
				}
				if(error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
					return error;
			}
		}

		static void ShowError(HMODULE module, HWND owner, const std::wstring &path, DWORD error)
		{
			wchar_t title[256]{}, description[512]{};
			::LoadStringW(module, IDS_PASTE_SYMBOLIC_LINK, title, _countof(title));
			::LoadStringW(module, IDS_SYMBOLIC_LINK_ERROR, description, _countof(description));
			wchar_t *systemMessage = nullptr;
			::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, error, 0, reinterpret_cast<wchar_t *>(&systemMessage), 0, nullptr);
			std::wstring text = std::wstring(description) + L"\n" + path + L"\n\n";
			text += systemMessage ? systemMessage : std::to_wstring(error);
			if(systemMessage)
				::LocalFree(systemMessage);
			::MessageBoxW(owner, text.c_str(), title, MB_OK | MB_ICONERROR);
		}
	};
}
