import unittest
from pathlib import Path


SOURCE = Path("os/services/network/network.c").read_text()


def function_body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    for index in range(opening + 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {signature}")


class NetworkWorkerStack(unittest.TestCase):
    def test_large_jobs_are_heap_owned_and_queue_carries_only_pointer(self):
        worker = function_body(SOURCE, "static void worker(void *arg)")
        self.assertRegex(worker, r"\bjob\s*\*j\s*=\s*NULL")
        self.assertNotRegex(worker, r"\bjob\s+j\b")
        self.assertIn("xQueueReceive(jobs, &j", worker)
        self.assertIn("job_free(j);", worker)
        self.assertIn("jobs = xQueueCreate(1, sizeof(job *));", SOURCE)
        self.assertIn("xQueueSend(jobs, &j, 0)", SOURCE)

    def test_request_entry_points_do_not_put_job_on_calling_stack(self):
        for signature in (
            "static bool enqueue(",
            "bool ps_net_request(",
            "bool ps_net_ws_open(",
        ):
            body = function_body(SOURCE, signature)
            self.assertRegex(body, r"\bjob\s*\*j\s*=\s*calloc")
            self.assertNotRegex(body, r"\bjob\s+j\s*=")


if __name__ == "__main__":
    unittest.main(verbosity=2)
