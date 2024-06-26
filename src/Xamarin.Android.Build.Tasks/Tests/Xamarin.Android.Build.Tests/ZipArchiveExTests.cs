using NUnit.Framework;
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Text;
using System.Threading;
using Xamarin.Android.Tasks;
using Xamarin.Tools.Zip;

namespace Xamarin.Android.Build.Tests
{
	[TestFixture]
	public class ZipArchiveExTests
	{
		ConcurrentDictionary<string, string> paths = new ConcurrentDictionary<string, string> ();
		string root;

		[SetUp]
		public void SetUp ()
		{
			root = Path.GetDirectoryName (GetType ().Assembly.Location);
			string temp = Path.Combine (root, "temp", TestContext.CurrentContext.Test.Name);
			if (Directory.Exists (temp))
				Directory.Delete (temp, recursive: true);
			if (File.Exists (Zip))
				File.Delete (Zip);
			Directory.CreateDirectory (temp);
			paths.TryAdd (TestContext.CurrentContext.Test.Name, temp);
		}

		[TearDown]
		public void TearDown ()
		{
			if (TestContext.CurrentContext.Result.Outcome.Status != NUnit.Framework.Interfaces.TestStatus.Passed) {
				return;
			}
			try {
				Directory.Delete (TestPath, recursive: true);
			} catch (Exception ex) {
				TestContext.WriteLine ($"Error deleting '{TestPath}': {ex}");
			}
			try {
				File.Delete (Zip);
			} catch (Exception ex) {
				TestContext.WriteLine ($"Error deleting '{Zip}': {ex}");
			}
			paths.TryRemove (TestContext.CurrentContext.Test.Name, out _);
		}

		public string TestPath {
			get {
				paths.TryGetValue (TestContext.CurrentContext.Test.Name, out string value);
				return value;
			}
		}

		public string Zip {
			get {
				return Path.Combine (root, "temp", $"{TestContext.CurrentContext.Test.Name}.zip");
			}
		}

		void CreateDirectories (params string [] paths)
		{
			foreach (var path in paths) {
				var dest = Path.Combine (TestPath, path);
				Directory.CreateDirectory (Path.GetDirectoryName (dest));
				//Just put the path in the test file, for testing purposes
				File.WriteAllText (dest, path);
			}
		}

		static DateTime WithoutMilliseconds (DateTime t) =>
			new DateTime (t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second, t.Kind);

		void AssertZip (string expected)
		{
			FileAssert.Exists (Zip, "Zip file should exist!");

			var builder = new StringBuilder ();
			using (var archive = new ZipArchiveEx (Zip, FileMode.Open)) {
				foreach (var entry in archive.Archive) {
					builder.AppendLine (entry.FullName);
				}
			}

			Assert.AreEqual (expected.Trim ().Replace("\r\n", "\n"), builder.ToString ().Trim ().Replace("\r\n", "\n"));
		}

		void AssertSkipExisting (string file, string fileInArchive, bool expected, CompressionMethod method = CompressionMethod.Deflate)
		{
			DateTime lastWrite =  File.GetLastWriteTimeUtc (file);
			string path = Path.GetFullPath (file);
			using (var archive = new ZipArchiveEx (Zip, FileMode.Open)) {
				var entry = archive.Archive.ContainsEntry (fileInArchive) ? archive.Archive.ReadEntry (fileInArchive) : null;
				var modTime = entry?.ModificationTime ?? DateTime.MinValue;
				var entryMethod = entry?.CompressionMethod;
				bool result = archive.SkipExistingFile (file, fileInArchive, method);
				Assert.AreEqual (expected, result,
					$"SkipExistingFile returned unexpected value for {Zip} {path} {fileInArchive}\n" +
					$"{method} != {entryMethod} or\n" +
					$"{WithoutMilliseconds(lastWrite)} (disk {lastWrite:MM/dd/yyyy HH:mm:ss:fff}) <= {WithoutMilliseconds (modTime)} (zip {modTime:MM/dd/yyyy HH:mm:ss:fff}) = {result}");
			}
		}

		[Test]
		public void AddDirectory ()
		{
			CreateDirectories ("A.txt", Path.Combine ("B", "B.txt"));

			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath, "temp", CompressionMethod.Deflate);
			}

			AssertZip (
@"temp/A.txt
temp/B/B.txt");
		}

		[Test]
		public void AddDirectoryOddPaths ()
		{
			CreateDirectories ("A.txt", Path.Combine ("B", "B.txt"));

			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath + $@"\../{nameof (AddDirectoryOddPaths)}/", "temp", CompressionMethod.Deflate);
			}

			AssertZip (
@"temp/A.txt
temp/B/B.txt");
		}

		[Test]
		public void AddDirectoryCurrentDirectory ()
		{
			CreateDirectories ("A.txt", Path.Combine ("B", "B.txt"));

			string cwd = Directory.GetCurrentDirectory ();
			try {
				Directory.SetCurrentDirectory (TestPath);

				using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
					archive.AddDirectory (".", "temp", CompressionMethod.Deflate);
				}

				AssertZip (
@"temp/A.txt
temp/B/B.txt");
			} finally {
				Directory.SetCurrentDirectory (cwd);
			}
		}

		[Test]
		[Repeat(100)]
		public void SkipExistingFile_Exist ()
		{
			CreateDirectories ("A.txt", Path.Combine ("B", "B.txt"));

			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath, folderInArchive: "", CompressionMethod.Deflate);
			}

			AssertZip (
@"A.txt
B/B.txt");
			AssertSkipExisting (Path.Combine (TestPath, "A.txt"), "A.txt", expected: true);
			AssertSkipExisting (Path.Combine (TestPath, "B", "B.txt"), "B/B.txt", expected: true);
			AssertSkipExisting (Path.Combine (TestPath, "C.txt"), "C.txt", expected: false);
			AssertSkipExisting (Path.Combine (TestPath, "C", "C.txt"), "C/C.txt", expected: false);
		}

		[Test]
		[Repeat(100)]
		public void SkipExistingFile_TimeStamp ()
		{
			CreateDirectories ("A.txt", Path.Combine ("B", "B.txt"));

			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath, folderInArchive: "", CompressionMethod.Deflate);
			}

			AssertZip (
@"A.txt
B/B.txt");
			var file = Path.Combine (TestPath, "A.txt");
			File.SetLastWriteTimeUtc (file, DateTime.UtcNow.AddSeconds (1));
			AssertSkipExisting (file, "A.txt", expected: false);
			AssertSkipExisting (Path.Combine (TestPath, "B", "B.txt"), "B/B.txt", expected: true);
		}

		[Test]
		[Repeat (100)]
		public void SkipExistingFile_Compression ()
		{
			var fileName = "A.txt";
			var filePath = Path.Combine (TestPath, fileName);
			CreateDirectories (fileName);

			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath, folderInArchive: "", CompressionMethod.Deflate);
			}

			AssertSkipExisting (filePath, fileName, expected: true, method: CompressionMethod.Deflate);
			AssertSkipExisting (filePath, fileName, expected: true, method: CompressionMethod.Default);
			AssertSkipExisting (filePath, fileName, expected: false, method: CompressionMethod.Store);

			File.Delete (Zip);
			using (var archive = new ZipArchiveEx (Zip, FileMode.Create)) {
				archive.AddDirectory (TestPath, folderInArchive: "", method: CompressionMethod.Store);
			}

			AssertSkipExisting (filePath, fileName, expected: true, method: CompressionMethod.Store);
			AssertSkipExisting (filePath, fileName, expected: false, method: CompressionMethod.Deflate);
			AssertSkipExisting (filePath, fileName, expected: false, method: CompressionMethod.Default);
		}
	}
}
